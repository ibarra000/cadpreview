#!/usr/bin/env python3
"""Validate the corpus manifest and run the cadpreview CLI across the corpus.

    python tools/corpus.py validate [--manifest corpus/manifest.csv]
    python tools/corpus.py run --cli build/win-release/bin/cadpreview.exe [--tier 0 --tier 5]

See corpus/README.md for the manifest rules.
"""

from __future__ import annotations

import argparse
import collections
import csv
import datetime
import hashlib
import json
import logging
import re
import subprocess
import sys
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, fields
from pathlib import Path, PurePosixPath

LOG = logging.getLogger("corpus")

REPO = Path(__file__).resolve().parent.parent
DEFAULT_MANIFEST = REPO / "corpus" / "manifest.csv"
SYNTHETIC = "synthetic"
ORIGINS = frozenset({"self-made", "open-license", SYNTHETIC})
FILE_DIRS = frozenset({"files", SYNTHETIC})
LFS_HEADER = b"version https://git-lfs.github.com/spec/v1\n"
LFS_POINTER_MAX = 1024
HASH_CHUNK = 1 << 20

FORMAT_RE = re.compile(r"^[a-z0-9]+(-[a-z0-9]+)*$")
SHA_RE = re.compile(r"^[0-9a-f]{64}$")
LICENSE_RE = re.compile(r"^[A-Za-z0-9.+-]+(?: (?:AND|OR|WITH) [A-Za-z0-9.+-]+)*$")
URL_RE = re.compile(r"^https?://\S+$")
LFS_OID_RE = re.compile(rb"^oid sha256:([0-9a-f]{64})$", re.MULTILINE)
LFS_SIZE_RE = re.compile(rb"^size (\d+)$", re.MULTILINE)


@dataclass(frozen=True)
class Entry:
    path: str
    format: str
    software: str
    file_version: str
    size_bytes: str
    sha256: str
    origin: str
    contributor: str
    license: str
    source: str
    date_added: str
    notes: str

    @property
    def is_synthetic(self) -> bool:
        return self.origin == SYNTHETIC


COLUMNS = tuple(f.name for f in fields(Entry))


@dataclass(frozen=True)
class Content:
    size: int
    sha256: str
    lfs_pointer: bool


def read_manifest(manifest: Path) -> tuple[list[tuple[int, Entry]], list[str]]:
    """Returns (line number, entry) pairs and header errors."""
    with manifest.open(newline="", encoding="utf-8") as handle:
        reader = csv.reader(handle)
        header = next(reader, [])
        if tuple(header) != COLUMNS:
            return [], [f"{manifest.name}: header must be: {','.join(COLUMNS)}"]
        entries: list[tuple[int, Entry]] = []
        for row in reader:
            if len(row) != len(COLUMNS):
                return entries, [
                    f"{manifest.name}:{reader.line_num}: expected {len(COLUMNS)} columns"
                ]
            entries.append((reader.line_num, Entry(*row)))
        return entries, []


def read_content(file: Path, lfs_allowed: bool) -> Content:
    """Size and SHA-256 of a file. Under files/ (lfs_allowed), an unfetched Git LFS pointer is
    read for the size and hash it records instead."""
    with file.open("rb") as handle:
        head = handle.read(LFS_POINTER_MAX + 1)
        if lfs_allowed and head.startswith(LFS_HEADER) and len(head) <= LFS_POINTER_MAX:
            oid = LFS_OID_RE.search(head)
            size = LFS_SIZE_RE.search(head)
            if oid and size:
                return Content(int(size.group(1)), oid.group(1).decode(), True)
        digest = hashlib.sha256(head)
        total = len(head)
        while chunk := handle.read(HASH_CHUNK):
            digest.update(chunk)
            total += len(chunk)
        return Content(total, digest.hexdigest(), False)


def check_path(entry: Entry) -> list[str]:
    path = PurePosixPath(entry.path)
    if not entry.path or path.is_absolute() or "\\" in entry.path or ".." in path.parts:
        return [f"path {entry.path!r} must be relative, use '/', and stay inside corpus/"]
    if len(path.parts) < 2 or path.parts[0] not in FILE_DIRS:
        return [f"path {entry.path!r} must start with files/ or synthetic/"]
    if (path.parts[0] == SYNTHETIC) != entry.is_synthetic:
        return ["synthetic files live in synthetic/ and nothing else does"]
    return []


def check_origin(entry: Entry) -> list[str]:
    if entry.origin not in ORIGINS:
        return [f"origin must be one of {sorted(ORIGINS)}"]
    errors = []
    if entry.origin == "self-made" and not (entry.contributor and entry.software):
        errors.append("self-made files need contributor and software")
    if entry.origin == "open-license" and not URL_RE.match(entry.source):
        errors.append("open-license files need an http(s) source URL")
    if entry.is_synthetic and not entry.source:
        errors.append("synthetic files need a source (spec section or generator)")
    return errors


def check_fields(entry: Entry) -> list[str]:
    errors = []
    if not FORMAT_RE.match(entry.format):
        errors.append(f"format {entry.format!r} must be a lowercase id such as 'dwg'")
    if not LICENSE_RE.match(entry.license):
        errors.append(f"license {entry.license!r} must be an SPDX expression")
    if not entry.size_bytes.isdigit():
        errors.append("size_bytes must be a non-negative integer")
    if not SHA_RE.match(entry.sha256):
        errors.append("sha256 must be 64 lowercase hex digits")
    try:
        datetime.date.fromisoformat(entry.date_added)
    except ValueError:
        errors.append("date_added must be YYYY-MM-DD")
    return errors


def lfs_allowed(entry: Entry) -> bool:
    return PurePosixPath(entry.path).parts[0] == "files"


def resolve_entry(entry: Entry, corpus_dir: Path) -> Path:
    """The entry's file, refusing symlinks and anything that resolves outside the corpus."""
    file = corpus_dir / entry.path
    if file.is_symlink():
        raise ValueError(f"{entry.path} is a symlink")
    if not file.resolve().is_relative_to(corpus_dir.resolve()):
        raise ValueError(f"{entry.path} resolves outside the corpus")
    return file


def check_file(entry: Entry, corpus_dir: Path) -> list[str]:
    try:
        file = resolve_entry(entry, corpus_dir)
    except ValueError as error:
        return [str(error)]
    if not file.is_file():
        return [f"file {entry.path} is missing"]
    content = read_content(file, lfs_allowed(entry))
    errors = []
    if entry.size_bytes.isdigit() and int(entry.size_bytes) != content.size:
        errors.append(f"size_bytes {entry.size_bytes} != actual {content.size}")
    if SHA_RE.match(entry.sha256) and entry.sha256 != content.sha256:
        errors.append("sha256 does not match the file")
    return errors


def check_entry(entry: Entry, corpus_dir: Path) -> list[str]:
    path_errors = check_path(entry)
    file_errors = [] if path_errors else check_file(entry, corpus_dir)
    return path_errors + check_origin(entry) + check_fields(entry) + file_errors


def find_orphans(corpus_dir: Path, listed: set[str]) -> list[str]:
    orphans = []
    for directory in sorted(FILE_DIRS):
        for file in sorted((corpus_dir / directory).rglob("*")):
            relative = file.relative_to(corpus_dir).as_posix()
            if file.is_file() and file.name != ".gitkeep" and relative not in listed:
                orphans.append(f"{relative} is not listed in the manifest")
    return orphans


def validate(manifest: Path) -> list[str]:
    """Every problem with the manifest and the files it lists; empty when valid."""
    entries, errors = read_manifest(manifest)
    corpus_dir = manifest.parent
    listed: set[str] = set()
    for line, entry in entries:
        problems = check_entry(entry, corpus_dir)
        if entry.path in listed:
            problems.append("duplicate path")
        listed.add(entry.path)
        errors.extend(f"{manifest.name}:{line}: {entry.path}: {p}" for p in problems)
    return errors + find_orphans(corpus_dir, listed)


@dataclass(frozen=True)
class RunResult:
    path: str
    format: str
    origin: str
    tier: int
    status: str  # ok, placeholder, crash, timeout, or skipped
    exit_code: int | None
    reason: str
    detail: str
    render_ms: float | None


def classify(entry: Entry, tier: int, proc: subprocess.CompletedProcess[bytes]) -> RunResult:
    lines = proc.stdout.decode("utf-8", "replace").strip().splitlines()
    record: dict[str, object] = {}
    if proc.returncode == 0 and lines:
        try:
            record = json.loads(lines[-1])
        except json.JSONDecodeError:
            record = {}
    if not record:
        stderr = proc.stderr.decode("utf-8", "replace").strip()
        return RunResult(
            entry.path,
            entry.format,
            entry.origin,
            tier,
            "crash",
            proc.returncode,
            "crash",
            stderr[:200],
            None,
        )
    render_ms = record.get("render_ms")
    return RunResult(
        entry.path,
        entry.format,
        entry.origin,
        tier,
        "ok" if record.get("ok") is True else "placeholder",
        proc.returncode,
        str(record.get("reason", "")),
        str(record.get("reason_detail", "")),
        float(render_ms) if isinstance(render_ms, (int, float)) else None,
    )


def run_one(
    cli: Sequence[str],
    corpus_dir: Path,
    entry: Entry,
    *,
    tier: int,
    size: int,
    timeout: float,
    out_dir: Path,
) -> RunResult:
    source = resolve_entry(entry, corpus_dir)
    if read_content(source, lfs_allowed(entry)).lfs_pointer:
        return RunResult(
            entry.path,
            entry.format,
            entry.origin,
            tier,
            "skipped",
            None,
            "lfs_not_fetched",
            "Git LFS object not fetched",
            None,
        )
    output = out_dir / f"tier{tier}" / f"{entry.path}.png"
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [*cli, str(source), "-o", str(output), "--size", str(size), "--tier", str(tier)]
    try:
        proc = subprocess.run(command, capture_output=True, timeout=timeout, check=False)
    except subprocess.TimeoutExpired:
        return RunResult(
            entry.path,
            entry.format,
            entry.origin,
            tier,
            "timeout",
            None,
            "timeout",
            f"exceeded {timeout:g} s",
            None,
        )
    return classify(entry, tier, proc)


def run_corpus(
    cli: Sequence[str],
    manifest: Path,
    *,
    tiers: Iterable[int],
    size: int,
    timeout: float,
    out_dir: Path,
) -> list[RunResult]:
    entries, errors = read_manifest(manifest)
    errors += [f"{entry.path}: {e}" for _, entry in entries for e in check_path(entry)]
    if errors:
        raise ValueError("; ".join(errors))
    return [
        run_one(cli, manifest.parent, entry, tier=tier, size=size, timeout=timeout, out_dir=out_dir)
        for tier in tiers
        for _, entry in entries
    ]


def summarize(results: Sequence[RunResult]) -> str:
    """Markdown table per format and tier. Synthetic files are listed but get no success rate."""
    groups: dict[tuple[str, int], list[RunResult]] = collections.defaultdict(list)
    for result in results:
        group = SYNTHETIC if result.origin == SYNTHETIC else result.format
        groups[(group, result.tier)].append(result)
    lines = [
        "| format | tier | files | ok | placeholder | crash | timeout | skipped | ok % "
        "| top reasons |",
        "|---|---|---|---|---|---|---|---|---|---|",
    ]
    for (group, tier), items in sorted(groups.items()):
        counts = collections.Counter(r.status for r in items)
        rated = len(items) - counts["skipped"]
        rate = "n/a" if group == SYNTHETIC or rated == 0 else f"{100 * counts['ok'] / rated:.0f}%"
        reasons = collections.Counter(r.reason for r in items if r.status not in ("ok", "skipped"))
        top = ", ".join(f"{name} ({n})" for name, n in reasons.most_common(3)) or "-"
        lines.append(
            f"| {group} | {tier} | {len(items)} | {counts['ok']} | {counts['placeholder']} | "
            f"{counts['crash']} | {counts['timeout']} | {counts['skipped']} | {rate} | {top} |"
        )
    return "\n".join(lines) + "\n"


def write_report(results: Sequence[RunResult], out_dir: Path) -> str:
    out_dir.mkdir(parents=True, exist_ok=True)
    with (out_dir / "results.jsonl").open("w", encoding="utf-8") as handle:
        for result in results:
            handle.write(json.dumps(result.__dict__) + "\n")
    summary = summarize(results)
    (out_dir / "summary.md").write_text(summary, encoding="utf-8")
    return summary


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("validate", help="check the manifest against the files")
    run = commands.add_parser("run", help="render every corpus file with the CLI")
    run.add_argument("--cli", type=Path, required=True, help="path to the cadpreview executable")
    run.add_argument(
        "--tier", type=int, action="append", choices=range(6), help="repeatable; default 0"
    )
    run.add_argument("--size", type=int, default=256)
    run.add_argument("--timeout", type=float, default=120.0, help="seconds per file")
    run.add_argument("--out", type=Path, help="default: build/corpus-run/<UTC timestamp>")
    run.add_argument(
        "--allow-skipped",
        action="store_true",
        help="do not fail when Git LFS objects are missing and files are skipped",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    args = parse_args(sys.argv[1:] if argv is None else argv)
    if args.command == "validate":
        errors = validate(args.manifest)
        for error in errors:
            LOG.error(error)
        LOG.info("manifest %s: %s", args.manifest.name, "invalid" if errors else "valid")
        return 1 if errors else 0
    stamp = datetime.datetime.now(datetime.UTC).strftime("%Y%m%dT%H%M%SZ")
    out_dir = args.out or REPO / "build" / "corpus-run" / stamp
    try:
        results = run_corpus(
            [str(args.cli)],
            args.manifest,
            tiers=args.tier or [0],
            size=args.size,
            timeout=args.timeout,
            out_dir=out_dir,
        )
    except ValueError as error:
        LOG.error("refusing to run: %s", error)
        return 1
    sys.stdout.write(write_report(results, out_dir))
    failing = {"crash", "timeout"} if args.allow_skipped else {"crash", "timeout", "skipped"}
    failures = [r for r in results if r.status in failing]
    for failure in failures:
        LOG.error("%s (tier %d): %s %s", failure.path, failure.tier, failure.status, failure.detail)
    LOG.info("results in %s", out_dir)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
