"""Behaviour tests for the cadpreview command-line tool.

CTest runs these with CADPREVIEW_CLI set to the built executable.
"""

import json
import os
import struct
import subprocess
from pathlib import Path
from typing import Any

import pytest

CLI = os.environ.get("CADPREVIEW_CLI", "")
FAKE_HELPER = os.environ.get("CADPREVIEW_FAKE_HELPER", "")
# Detected as DWG (AC1024) with no embedded preview, so only tier 4 can draw it.
DWG_WITHOUT_PREVIEW = b"AC1024" + bytes(122)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
EXIT_USAGE = 2
REQUIRED_FIELDS = {
    "input",
    "output",
    "format",
    "version",
    "tier",
    "ok",
    "render_ms",
    "reason",
    "reason_detail",
    "warnings",
}

pytestmark = pytest.mark.skipif(not CLI, reason="CADPREVIEW_CLI is not set")


def run_cli(*args: object) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run([CLI, *map(str, args)], capture_output=True, timeout=120, check=False)


def png_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    assert data[:8] == PNG_SIGNATURE and data[12:16] == b"IHDR", f"{path} is not a PNG"
    width, height = struct.unpack(">II", data[16:24])
    return width, height


def parse_result(result: subprocess.CompletedProcess[bytes]) -> dict[str, Any]:
    assert result.returncode == 0, result.stderr
    lines = result.stdout.decode("utf-8").splitlines()
    assert len(lines) == 1
    record: dict[str, Any] = json.loads(lines[0])
    assert set(record) == REQUIRED_FIELDS
    return record


def render_bytes(
    tmp_path: Path, data: bytes, *extra: object
) -> tuple[subprocess.CompletedProcess[bytes], Path]:
    source = tmp_path / "input.bin"
    source.write_bytes(data)
    output = tmp_path / "out.png"
    return run_cli(source, "-o", output, *extra), output


def assert_usage_error(*args: object) -> None:
    result = run_cli(*args)
    assert result.returncode == EXIT_USAGE, result.stderr
    assert result.stdout == b""
    assert b"cadpreview:" in result.stderr


@pytest.mark.parametrize(
    "args",
    [
        (),
        ("input.bin",),
        ("input.bin", "-o", "out.png", "--colour", "red"),
        ("a.bin", "b.bin", "-o", "out.png"),
        ("input.bin", "-o"),
        ("input.bin", "-o", "out.png", "--dwg-helper"),
    ],
)
def test_malformed_command_lines_are_usage_errors(args: tuple[str, ...]) -> None:
    assert_usage_error(*args)


@pytest.mark.parametrize("size", ["abc", "-5", "8", "5000", ""])
def test_bad_sizes_are_usage_errors(tmp_path: Path, size: str) -> None:
    assert_usage_error(tmp_path / "in.bin", "-o", tmp_path / "o.png", "--size", size)


@pytest.mark.parametrize("tier", ["6", "x", "1.5"])
def test_bad_tiers_are_usage_errors(tmp_path: Path, tier: str) -> None:
    assert_usage_error(tmp_path / "in.bin", "-o", tmp_path / "o.png", "--tier", tier)


def test_unwritable_output_is_a_usage_error(tmp_path: Path) -> None:
    result = run_cli(tmp_path / "in.bin", "-o", tmp_path / "no-such-dir" / "out.png")
    assert result.returncode == EXIT_USAGE
    assert b"cannot write" in result.stderr


def test_refuses_to_overwrite_the_input(tmp_path: Path) -> None:
    source = tmp_path / "model.dwg"
    source.write_bytes(b"precious CAD data")
    result = run_cli(source, "-o", source)
    assert result.returncode == EXIT_USAGE
    assert b"refusing to overwrite" in result.stderr
    assert source.read_bytes() == b"precious CAD data"


def test_replaces_an_existing_output_without_leftovers(tmp_path: Path) -> None:
    output = tmp_path / "out.png"
    output.write_bytes(b"old")
    result, _ = render_bytes(tmp_path, b"x")
    parse_result(result)
    assert output.read_bytes().startswith(PNG_SIGNATURE)
    assert sorted(p.name for p in tmp_path.iterdir()) == ["input.bin", "out.png"]


def test_help() -> None:
    result = run_cli("--help")
    assert result.returncode == 0
    assert b"usage: cadpreview" in result.stdout


def test_version() -> None:
    result = run_cli("--version")
    assert result.returncode == 0
    assert result.stdout.decode().startswith("cadpreview ")
    assert result.stdout.decode().split()[1].count(".") == 2


def test_missing_input_still_writes_a_placeholder(tmp_path: Path) -> None:
    output = tmp_path / "out.png"
    record = parse_result(run_cli(tmp_path / "missing.sldprt", "-o", output))
    assert record["ok"] is False
    assert record["tier"] == 5
    assert record["reason"] == "cannot_open"
    assert record["version"] is None
    assert png_dimensions(output) == (512, 512)


def test_unknown_bytes_give_an_unknown_format_placeholder(tmp_path: Path) -> None:
    result, output = render_bytes(tmp_path, b"not a cad file")
    record = parse_result(result)
    assert record["format"] == "unknown"
    assert record["reason"] == "unknown_format"
    assert record["warnings"] == []
    assert record["render_ms"] >= 0
    assert output.exists()


def test_size_option_sets_png_dimensions(tmp_path: Path) -> None:
    result, output = render_bytes(tmp_path, b"x", "--size", "128")
    parse_result(result)
    assert png_dimensions(output) == (128, 128)


def test_forced_placeholder_tier(tmp_path: Path) -> None:
    result, _ = render_bytes(tmp_path, b"x", "--tier", "5")
    assert parse_result(result)["reason"] == "placeholder_requested"


def test_missing_dwg_helper_makes_the_vector_tier_unavailable(tmp_path: Path) -> None:
    missing = tmp_path / "no-such-helper"
    result, _ = render_bytes(tmp_path, DWG_WITHOUT_PREVIEW, "--tier", "4", "--dwg-helper", missing)
    record = parse_result(result)
    assert record["format"] == "dwg"
    assert record["reason"] == "tier_unavailable"
    assert "cadpreview-dwg" in record["reason_detail"]


@pytest.mark.skipif(not FAKE_HELPER, reason="CADPREVIEW_FAKE_HELPER is not set")
def test_dwg_helper_option_draws_through_the_helper(tmp_path: Path) -> None:
    result, output = render_bytes(
        tmp_path, DWG_WITHOUT_PREVIEW, "--tier", "4", "--dwg-helper", FAKE_HELPER
    )
    record = parse_result(result)
    assert record["ok"] is True
    assert record["tier"] == 4
    assert output.exists()


def test_verbose_logs_go_to_stderr(tmp_path: Path) -> None:
    result, _ = render_bytes(tmp_path, b"x", "--verbose")
    parse_result(result)
    assert b"[info]" in result.stderr


def test_non_ascii_paths(tmp_path: Path) -> None:
    source = tmp_path / "pièce-零件.bin"
    source.write_bytes(b"data")
    output = tmp_path / "aperçu-预览.png"
    record = parse_result(run_cli(source, "-o", output))
    assert record["reason"] == "unknown_format"
    assert record["input"] == str(source)
    assert output.exists()
