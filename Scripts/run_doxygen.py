#!/usr/bin/env python3
"""Build API docs from the site entry page and every current Public directory."""

from __future__ import annotations

import subprocess
import sys

from check_text import ROOT, repository_files


def main() -> int:
    if len(sys.argv) > 2 or (len(sys.argv) == 2 and sys.argv[1] != "--publish"):
        print("Usage: run_doxygen.py [--publish]", file=sys.stderr)
        return 2
    publishing = len(sys.argv) == 2
    (ROOT / ".tools").mkdir(exist_ok=True)
    headers = [
        path for path in repository_files()
        if path.is_file() and path.suffix.lower() in {".h", ".hpp"} and "Public" in path.relative_to(ROOT).parts
    ]
    public_dirs = sorted({path.parent for path in headers})
    config = (ROOT / "Doxyfile").read_text(encoding="utf-8")
    for directory in public_dirs:
        config += f'\nINPUT += "{directory.relative_to(ROOT).as_posix()}"'
    if publishing:
        config += "\nWARN_AS_ERROR = NO"
    config += "\n"
    try:
        result = subprocess.run(["doxygen", "-"], input=config, text=True, cwd=ROOT, check=False)
    except FileNotFoundError:
        print("Doxygen is required (pin: 1.18.0).", file=sys.stderr)
        return 1
    if result.returncode == 0:
        print(f"Built API docs from {len(public_dirs)} public header directories.")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
