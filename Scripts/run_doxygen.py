#!/usr/bin/env python3
# Copyright (c) 2026 Nelaric
"""Build the documentation site from the project coding standards."""

from __future__ import annotations

import subprocess
import sys

from check_text import ROOT


def main() -> int:
    if len(sys.argv) > 2 or (len(sys.argv) == 2 and sys.argv[1] != "--publish"):
        print("Usage: run_doxygen.py [--publish]", file=sys.stderr)
        return 2
    publishing = len(sys.argv) == 2
    (ROOT / ".tools").mkdir(exist_ok=True)
    config = (ROOT / "Doxyfile").read_text(encoding="utf-8")
    if publishing:
        config += "\nWARN_AS_ERROR = NO"
    config += "\n"
    try:
        result = subprocess.run(["doxygen", "-"], input=config, text=True, cwd=ROOT, check=False)
    except FileNotFoundError:
        print("Doxygen is required (pin: 1.18.0).", file=sys.stderr)
        return 1
    if result.returncode == 0:
        print("Built documentation from the project coding standards.")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())
