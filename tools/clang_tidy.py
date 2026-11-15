#!/usr/bin/env python3
"""Run clang-tidy over the project's C++ sources using a CMake compile database.

    python tools/clang_tidy.py [--build-dir build/linux-debug] [FILE ...]

Without --build-dir the most recently configured build/*/ directory is used. Without files,
every project source listed in the compile database is checked.
"""

from __future__ import annotations

import argparse
import json
import logging
import os
import shutil
import subprocess
import sys
from collections.abc import Sequence
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

LOG = logging.getLogger("clang-tidy")

REPO = Path(__file__).resolve().parent.parent
PROJECT_DIRS = ("src", "cli", "tests", "fuzz", "helpers")
SOURCE_SUFFIXES = (".cpp",)


def newest_build_dir() -> Path:
    databases = sorted(
        REPO.glob("build/*/compile_commands.json"), key=lambda p: p.stat().st_mtime, reverse=True
    )
    if not databases:
        raise FileNotFoundError("no build/*/compile_commands.json; configure a preset first")
    return databases[0].parent


def project_sources(build_dir: Path) -> list[Path]:
    entries = json.loads((build_dir / "compile_commands.json").read_text(encoding="utf-8"))
    roots = [REPO / name for name in PROJECT_DIRS]
    sources = {Path(entry["file"]).resolve() for entry in entries}
    return sorted(
        s
        for s in sources
        if s.suffix in SOURCE_SUFFIXES and any(s.is_relative_to(r) for r in roots)
    )


def tidy(executable: str, build_dir: Path, source: Path) -> tuple[Path, int, str]:
    result = subprocess.run(
        [executable, "-p", str(build_dir), "--quiet", str(source)],
        capture_output=True,
        text=True,
        check=False,
    )
    return source, result.returncode, (result.stdout + result.stderr).strip()


def main(argv: Sequence[str] | None = None) -> int:
    logging.basicConfig(level=logging.INFO, format="%(message)s")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("files", nargs="*", type=Path)
    args = parser.parse_args(sys.argv[1:] if argv is None else argv)

    executable = shutil.which("clang-tidy")
    if executable is None:
        LOG.error("clang-tidy not found; pip install -r requirements-dev.txt")
        return 1
    build_dir = (args.build_dir or newest_build_dir()).resolve()
    known = project_sources(build_dir)
    wanted = {f.resolve() for f in args.files}
    sources = [s for s in known if not wanted or s in wanted]

    failures = 0
    with ThreadPoolExecutor(max_workers=os.cpu_count() or 2) as pool:
        for source, code, output in pool.map(lambda s: tidy(executable, build_dir, s), sources):
            if code != 0:
                failures += 1
                LOG.error("%s\n%s", source.relative_to(REPO), output)
    LOG.info("clang-tidy: %d file(s) checked, %d with findings", len(sources), failures)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
