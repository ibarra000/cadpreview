#!/usr/bin/env python3
"""Pack the CadPreview NuGet package and smoke-test it in a fresh console app.

    python tools/nuget.py pack --stage win-x64=build/win-release/stage [--stage win-arm64=DIR]
                               [--version-suffix ci.42] [--output build/nuget]
    python tools/nuget.py smoke [--feed build/nuget] [--version 0.1.0-ci.42] [--input FILE]

A stage directory is a `cmake --install` prefix. The package version comes from the staged
include/cadpreview/version.h, so the CMake project version stays the single source.
"""

from __future__ import annotations

import argparse
import logging
import os
import re
import subprocess
import sys
import tempfile
from collections.abc import Sequence
from pathlib import Path

LOG = logging.getLogger("nuget")

REPO = Path(__file__).resolve().parent.parent
PROJECT = REPO / "bindings" / "dotnet" / "src" / "CadPreview" / "CadPreview.csproj"
SMOKE_PROJECT = REPO / "tests" / "nuget-smoke" / "NuGetSmoke.csproj"
NATIVE_PROPERTIES = {"win-x64": "CadPreviewNativeWinX64", "win-arm64": "CadPreviewNativeWinArm64"}
# IMAGE_FILE_HEADER.Machine values from the PE format specification.
PE_MACHINES = {"win-x64": 0x8664, "win-arm64": 0xAA64}
VERSION_RE = re.compile(r'#define CP_VERSION_STRING "(\d+\.\d+\.\d+)"')
SUFFIX_RE = re.compile(r"^[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*$")
PACKAGE_VERSION_RE = re.compile(r"^\d+\.\d+\.\d+(-[0-9A-Za-z-]+(\.[0-9A-Za-z-]+)*)?$")
PE_OFFSET_FIELD = 0x3C


def staged_version(stage: Path) -> str:
    header = stage / "include" / "cadpreview" / "version.h"
    match = VERSION_RE.search(header.read_text(encoding="utf-8"))
    if not match:
        raise ValueError(f"no CP_VERSION_STRING in {header}")
    return match.group(1)


def pe_machine(dll: Path) -> int:
    """The Machine field of a PE file's COFF header."""
    data = dll.read_bytes()
    if data[:2] != b"MZ" or len(data) < PE_OFFSET_FIELD + 4:
        raise ValueError(f"{dll} is not a PE file")
    header = int.from_bytes(data[PE_OFFSET_FIELD : PE_OFFSET_FIELD + 4], "little")
    if data[header : header + 4] != b"PE\0\0":
        raise ValueError(f"{dll} is not a PE file")
    return int.from_bytes(data[header + 4 : header + 6], "little")


def parse_stages(values: Sequence[str], release: bool) -> dict[str, Path]:
    stages: dict[str, Path] = {}
    for value in values:
        rid, _, directory = value.partition("=")
        if rid not in NATIVE_PROPERTIES or not directory:
            raise ValueError(f"--stage must be RID=DIR with RID in {sorted(NATIVE_PROPERTIES)}")
        stages[rid] = Path(directory).resolve()
        dll = stages[rid] / "bin" / "cadpreview.dll"
        if not dll.is_file():
            raise ValueError(f"{stages[rid]} has no bin/cadpreview.dll")
        if pe_machine(dll) != PE_MACHINES[rid]:
            raise ValueError(f"{dll} is not a {rid} binary")
    required = set(NATIVE_PROPERTIES) if release else {"win-x64"}
    if missing := required - set(stages):
        raise ValueError(f"missing stages: {sorted(missing)}")
    return stages


def package_version(stages: dict[str, Path], suffix: str | None) -> str:
    versions = {staged_version(stage) for stage in stages.values()}
    if len(versions) != 1:
        raise ValueError(f"stages disagree on the version: {sorted(versions)}")
    if suffix and not SUFFIX_RE.match(suffix):
        raise ValueError(f"invalid prerelease suffix {suffix!r}")
    base = versions.pop()
    return f"{base}-{suffix}" if suffix else base


def run(command: Sequence[str], env: dict[str, str] | None = None) -> None:
    LOG.info("$ %s", " ".join(command))
    subprocess.run(command, check=True, env=env)


def pack(stages: dict[str, Path], suffix: str | None, output: Path) -> Path:
    version = package_version(stages, suffix)
    for stale in output.glob("CadPreview.*.nupkg"):
        stale.unlink()
    properties = [f"-p:{NATIVE_PROPERTIES[rid]}={stage / 'bin'}" for rid, stage in stages.items()]
    run(
        [
            "dotnet",
            "pack",
            str(PROJECT),
            "--configuration",
            "Release",
            "--output",
            str(output),
            "--artifacts-path",
            str(output.parent / "nuget-build"),
            f"-p:Version={version}",
            *properties,
        ]
    )
    package = output / f"CadPreview.{version}.nupkg"
    LOG.info("packed %s (%.1f KiB)", package, package.stat().st_size / 1024)
    return package


def feed_version(feed: Path) -> str:
    packages = sorted(feed.glob("CadPreview.*.nupkg"))
    if len(packages) != 1:
        raise ValueError(f"expected one CadPreview package in {feed}, found {len(packages)}")
    return packages[0].name.removeprefix("CadPreview.").removesuffix(".nupkg")


def smoke(feed: Path, version: str | None, input_file: Path) -> None:
    """Restores the package into an isolated cache and renders input_file with it."""
    version = version or feed_version(feed)
    if not PACKAGE_VERSION_RE.match(version):
        raise ValueError(f"invalid package version {version!r}")
    # The .NET build server can keep files in the package cache open briefly after the run.
    with tempfile.TemporaryDirectory(
        prefix="cadpreview-smoke-", ignore_cleanup_errors=True
    ) as scratch:
        env = {**os.environ, "NUGET_PACKAGES": str(Path(scratch) / "packages")}
        output = Path(scratch) / "smoke.png"
        version_property = f"-p:CadPreviewVersion={version}"
        run(
            [
                "dotnet",
                "restore",
                str(SMOKE_PROJECT),
                "--source",
                str(feed.resolve()),
                version_property,
            ],
            env,
        )
        run(
            [
                "dotnet",
                "run",
                "--project",
                str(SMOKE_PROJECT),
                "--no-restore",
                version_property,
                "--",
                str(input_file),
                str(output),
                version.split("-")[0],
            ],
            env,
        )


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    commands = parser.add_subparsers(dest="command", required=True)
    pack_cmd = commands.add_parser("pack")
    pack_cmd.add_argument("--stage", action="append", required=True, help="RID=DIR, repeatable")
    pack_cmd.add_argument("--version-suffix", help="prerelease suffix such as ci.42")
    pack_cmd.add_argument("--output", type=Path, default=REPO / "build" / "nuget")
    smoke_cmd = commands.add_parser("smoke")
    smoke_cmd.add_argument("--feed", type=Path, default=REPO / "build" / "nuget")
    smoke_cmd.add_argument("--version", help="default: the only package in the feed")
    smoke_cmd.add_argument("--input", type=Path, default=REPO / "README.md")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    args = parse_args(sys.argv[1:] if argv is None else argv)
    try:
        if args.command == "pack":
            stages = parse_stages(args.stage, release=not args.version_suffix)
            pack(stages, args.version_suffix, args.output.resolve())
        else:
            smoke(args.feed, args.version, args.input)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        LOG.error("%s", error)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
