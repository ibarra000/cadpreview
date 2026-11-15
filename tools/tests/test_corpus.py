"""Tests for tools/corpus.py."""

import csv
import hashlib
import sys
import textwrap
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import corpus

GOOD_BYTES = b"synthetic fixture\n"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def row(**overrides: str) -> dict[str, str]:
    values = {
        "path": "synthetic/good.bin",
        "format": "unknown",
        "software": "",
        "file_version": "",
        "size_bytes": str(len(GOOD_BYTES)),
        "sha256": sha256(GOOD_BYTES),
        "origin": "synthetic",
        "contributor": "tests",
        "license": "Apache-2.0",
        "source": "generated: test",
        "date_added": "2026-11-14",
        "notes": "",
    }
    values.update(overrides)
    return values


def write_corpus(root: Path, rows: list[dict[str, str]], files: dict[str, bytes]) -> Path:
    root.mkdir(parents=True, exist_ok=True)
    for relative, data in files.items():
        target = root / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    manifest = root / "manifest.csv"
    with manifest.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=corpus.COLUMNS)
        writer.writeheader()
        writer.writerows(rows)
    return manifest


@pytest.fixture
def good_files() -> dict[str, bytes]:
    return {"synthetic/good.bin": GOOD_BYTES}


def test_valid_manifest_has_no_errors(tmp_path: Path, good_files: dict[str, bytes]) -> None:
    assert corpus.validate(write_corpus(tmp_path, [row()], good_files)) == []


def test_repository_manifest_is_valid() -> None:
    assert corpus.validate(corpus.DEFAULT_MANIFEST) == []


def test_wrong_header_is_reported(tmp_path: Path) -> None:
    manifest = tmp_path / "manifest.csv"
    manifest.write_text("path,format\n", encoding="utf-8")
    assert "header" in corpus.validate(manifest)[0]


@pytest.mark.parametrize(
    ("overrides", "message"),
    [
        ({"origin": "found-online"}, "origin must be one of"),
        ({"origin": "self-made"}, "self-made files live in files/"),
        ({"path": "../escape.bin"}, "must be relative"),
        ({"path": "elsewhere/good.bin"}, "must start with one of"),
        ({"sha256": "0" * 64}, "sha256 does not match"),
        ({"size_bytes": "999"}, "size_bytes 999 != actual"),
        ({"size_bytes": "-1"}, "non-negative integer"),
        ({"format": "DWG"}, "lowercase id"),
        ({"license": "some license"}, "SPDX"),
        ({"date_added": "14/11/2026"}, "YYYY-MM-DD"),
        ({"source": ""}, "need a source"),
    ],
)
def test_bad_rows_are_reported(
    tmp_path: Path, good_files: dict[str, bytes], overrides: dict[str, str], message: str
) -> None:
    errors = corpus.validate(write_corpus(tmp_path, [row(**overrides)], good_files))
    assert any(message in e for e in errors), errors


def test_self_made_needs_contributor_and_software(tmp_path: Path) -> None:
    files = {"files/dwg/a.dwg": GOOD_BYTES}
    bad = row(path="files/dwg/a.dwg", format="dwg", origin="self-made", contributor="")
    errors = corpus.validate(write_corpus(tmp_path, [bad], files))
    assert any("contributor and software" in e for e in errors), errors


def test_open_license_needs_url(tmp_path: Path) -> None:
    files = {"files/dxf/a.dxf": GOOD_BYTES}
    bad = row(path="files/dxf/a.dxf", format="dxf", origin="open-license", source="a forum")
    errors = corpus.validate(write_corpus(tmp_path, [bad], files))
    assert any("source URL" in e for e in errors), errors


def test_missing_orphan_and_duplicate_files(tmp_path: Path, good_files: dict[str, bytes]) -> None:
    files = {**good_files, "synthetic/orphan.bin": b"x"}
    rows = [row(), row(), row(path="synthetic/missing.bin")]
    errors = corpus.validate(write_corpus(tmp_path, rows, files))
    assert any("duplicate path" in e for e in errors)
    assert any("missing.bin is missing" in e for e in errors)
    assert any("orphan.bin is not listed" in e for e in errors)


def lfs_pointer(data: bytes) -> bytes:
    return textwrap.dedent(f"""\
        version https://git-lfs.github.com/spec/v1
        oid sha256:{sha256(data)}
        size {len(data)}
        """).encode()


def test_unfetched_lfs_pointer_is_checked_against_its_oid(tmp_path: Path) -> None:
    real = b"real CAD bytes " * 100
    files = {"files/prt/a.prt": lfs_pointer(real)}
    good = row(
        path="files/prt/a.prt",
        format="nx-part",
        origin="self-made",
        software="test",
        size_bytes=str(len(real)),
        sha256=sha256(real),
    )
    assert corpus.validate(write_corpus(tmp_path, [good], files)) == []
    wrong = {**good, "sha256": sha256(b"other")}
    assert corpus.validate(write_corpus(tmp_path, [wrong], files)) != []


def test_lfs_pointer_outside_files_is_hashed_as_is(tmp_path: Path) -> None:
    pointer = lfs_pointer(b"hidden")
    claims_hidden = row(size_bytes=str(len(b"hidden")), sha256=sha256(b"hidden"))
    errors = corpus.validate(
        write_corpus(tmp_path, [claims_hidden], {"synthetic/good.bin": pointer})
    )
    assert any("sha256 does not match" in e for e in errors), errors


def test_run_refuses_paths_that_escape_the_corpus(tmp_path: Path) -> None:
    manifest = write_corpus(tmp_path / "corpus", [row(path="../outside.bin")], {})
    with pytest.raises(ValueError, match="stay inside corpus"):
        corpus.run_corpus(
            fake_cli(tmp_path), manifest, tiers=[0], size=64, timeout=5.0, out_dir=tmp_path / "o"
        )


def test_symlinks_are_rejected(tmp_path: Path, good_files: dict[str, bytes]) -> None:
    manifest = write_corpus(tmp_path, [row()], good_files)
    target = tmp_path / "synthetic" / "good.bin"
    real = tmp_path / "real.bin"
    target.replace(real)
    try:
        target.symlink_to(real)
    except OSError:
        pytest.skip("creating symlinks needs extra privileges on this system")
    assert any("symlink" in e for e in corpus.validate(manifest))


FAKE_CLI = """\
import json, pathlib, sys, time
args = sys.argv[1:]
data = pathlib.Path(args[0]).read_bytes()
if data.startswith(b"crash"):
    sys.exit(3)
if data.startswith(b"slow"):
    time.sleep(30)
pathlib.Path(args[args.index("-o") + 1]).write_bytes(b"png")
ok = data.startswith(b"good")
print(json.dumps({"ok": ok, "reason": "none" if ok else "unknown_format",
                  "reason_detail": "", "render_ms": 1.5}))
"""


def fake_cli(tmp_path: Path) -> list[str]:
    script = tmp_path / "fake_cli.py"
    script.write_text(FAKE_CLI, encoding="utf-8")
    return [sys.executable, str(script)]


def test_run_classifies_results_and_summarizes(tmp_path: Path) -> None:
    contents = {
        "files/dwg/good.dwg": b"good",
        "files/dwg/bad.dwg": b"bad",
        "files/dwg/crash.dwg": b"crash",
        "files/dwg/slow.dwg": b"slow",
        "synthetic/x.bin": b"bad",
    }
    rows = [
        row(
            path=p,
            format="dwg",
            origin="self-made",
            software="t",
            size_bytes=str(len(d)),
            sha256=sha256(d),
        )
        for p, d in contents.items()
        if p.startswith("files/")
    ]
    rows.append(row(path="synthetic/x.bin", size_bytes="3", sha256=sha256(b"bad")))
    manifest = write_corpus(tmp_path / "corpus", rows, contents)

    results = corpus.run_corpus(
        fake_cli(tmp_path), manifest, tiers=[0], size=64, timeout=5.0, out_dir=tmp_path / "out"
    )
    status = {r.path: r.status for r in results}
    assert status == {
        "files/dwg/good.dwg": "ok",
        "files/dwg/bad.dwg": "placeholder",
        "files/dwg/crash.dwg": "crash",
        "files/dwg/slow.dwg": "timeout",
        "synthetic/x.bin": "placeholder",
    }

    summary = corpus.write_report(results, tmp_path / "out")
    assert "| dwg | 0 | 4 | 1 | 1 | 1 | 1 | 0 | 25% |" in summary
    assert "| synthetic | 0 | 1 | 0 | 1 | 0 | 0 | 0 | n/a |" in summary
    assert (tmp_path / "out" / "results.jsonl").read_text().count("\n") == 5


def test_main_validate_exit_codes(tmp_path: Path, good_files: dict[str, bytes]) -> None:
    good = write_corpus(tmp_path / "a", [row()], good_files)
    bad = write_corpus(tmp_path / "b", [row(origin="?")], good_files)
    assert corpus.main(["--manifest", str(good), "validate"]) == 0
    assert corpus.main(["--manifest", str(bad), "validate"]) == 1


def test_main_run_fails_on_crash(tmp_path: Path) -> None:
    data = b"crash"
    manifest = write_corpus(
        tmp_path / "corpus",
        [row(path="synthetic/c.bin", size_bytes="5", sha256=sha256(data))],
        {"synthetic/c.bin": data},
    )
    cli = tmp_path / "cli.cmd" if sys.platform == "win32" else tmp_path / "cli.sh"
    script = fake_cli(tmp_path)
    if sys.platform == "win32":
        cli.write_text(f'@"{script[0]}" "{script[1]}" %*\n', encoding="utf-8")
    else:
        cli.write_text(f'#!/bin/sh\nexec "{script[0]}" "{script[1]}" "$@"\n', encoding="utf-8")
        cli.chmod(0o755)
    code = corpus.main(
        ["--manifest", str(manifest), "run", "--cli", str(cli), "--out", str(tmp_path / "o")]
    )
    assert code == 1


def test_vendor_samples_may_be_absent_but_must_name_their_url(tmp_path: Path) -> None:
    absent = row(
        path="vendor/dwg/sample.dwg",
        format="dwg",
        origin="vendor-sample",
        license="LicenseRef-Vendor-Terms",
        source="https://example.com/samples",
    )
    assert corpus.validate(write_corpus(tmp_path / "a", [absent], {})) == []
    no_url = {**absent, "source": "the vendor site"}
    errors = corpus.validate(write_corpus(tmp_path / "b", [no_url], {}))
    assert any("downloaded from" in e for e in errors), errors
    misplaced = {**absent, "path": "files/dwg/sample.dwg"}
    errors = corpus.validate(write_corpus(tmp_path / "c", [misplaced], {}))
    assert any("vendor-sample files live in vendor/" in e for e in errors), errors


def test_run_skips_absent_vendor_samples(tmp_path: Path) -> None:
    absent = row(
        path="vendor/dwg/sample.dwg",
        format="dwg",
        origin="vendor-sample",
        license="LicenseRef-Vendor-Terms",
        source="https://example.com/samples",
    )
    manifest = write_corpus(tmp_path / "corpus", [absent], {})
    assert (
        corpus.run_corpus(
            fake_cli(tmp_path), manifest, tiers=[0], size=64, timeout=5.0, out_dir=tmp_path / "o"
        )
        == []
    )


def test_add_appends_rows_and_refuses_duplicates(tmp_path: Path) -> None:
    manifest = write_corpus(tmp_path / "corpus", [], {"synthetic/new.dxf": b"0\nEOF\n"})
    new = corpus.NewEntry(
        origin="synthetic",
        source="generated: test",
        license="Apache-2.0",
        contributor="tests",
        format="dxf",
    )
    added = corpus.add_entries(manifest, [tmp_path / "corpus" / "synthetic" / "new.dxf"], new)
    assert added == ["synthetic/new.dxf"]
    assert corpus.validate(manifest) == []
    with pytest.raises(ValueError, match="already listed"):
        corpus.add_entries(manifest, [tmp_path / "corpus" / "synthetic" / "new.dxf"], new)
    with pytest.raises(ValueError, match="inside"):
        corpus.add_entries(manifest, [tmp_path / "elsewhere.dxf"], new)


def test_add_can_ask_the_cli_for_format_and_version(tmp_path: Path) -> None:
    script = tmp_path / "probe_cli.py"
    script.write_text(
        "import json, pathlib, sys\n"
        "pathlib.Path(sys.argv[sys.argv.index('-o') + 1]).write_bytes(b'png')\n"
        "print(json.dumps({'format': 'dwg', 'version': 'AC1032'}))\n",
        encoding="utf-8",
    )
    manifest = write_corpus(tmp_path / "corpus", [], {"synthetic/x.bin": b"AC1032"})
    new = corpus.NewEntry(origin="synthetic", source="generated: test", license="Apache-2.0")
    corpus.add_entries(
        manifest, [tmp_path / "corpus" / "synthetic" / "x.bin"], new, [sys.executable, str(script)]
    )
    entries, _ = corpus.read_manifest(manifest)
    assert (entries[0][1].format, entries[0][1].file_version) == ("dwg", "AC1032")
