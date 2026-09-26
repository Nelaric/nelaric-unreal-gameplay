#!/usr/bin/env python3
# Copyright (c) 2026 Nelaric
"""Check the repository's explicit UTF-8 and newline rules without rewriting files."""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOM = b"\xef\xbb\xbf"
CRLF_SUFFIXES = {".md", ".h", ".hpp", ".cpp", ".cc", ".cxx", ".cs"}
LF_SUFFIXES = {".json", ".yml", ".yaml", ".py"}
LF_NAMES = {".gitattributes", ".gitignore", ".editorconfig", ".clang-format", ".csharpierrc", "Doxyfile"}
VENDORED_ROOTS = {Path("NelaricGameplay/Plugins/Puerts")}


def repository_files() -> list[Path]:
    result = subprocess.run(
        ["git", "ls-files", "--cached", "--others", "--exclude-standard", "-z"],
        cwd=ROOT,
        check=True,
        capture_output=True,
    )
    return [
        ROOT / relative
        for name in result.stdout.split(b"\0")
        if name
        for relative in [Path(name.decode("utf-8"))]
        if not any(relative.is_relative_to(vendor) for vendor in VENDORED_ROOTS)
    ]


def main() -> int:
    errors: list[str] = []
    checked = 0
    for path in repository_files():
        if not path.is_file():
            continue
        if path.suffix.lower() in CRLF_SUFFIXES or path.name == "LICENSE":
            expect_bom, expect_crlf = True, True
        elif path.suffix.lower() in LF_SUFFIXES or path.name in LF_NAMES:
            expect_bom, expect_crlf = False, False
        else:
            continue

        checked += 1
        data = path.read_bytes()
        label = path.relative_to(ROOT).as_posix()
        if data.startswith(BOM) != expect_bom:
            errors.append(f"{label}: expected {'UTF-8 BOM' if expect_bom else 'no BOM'}")
        try:
            data.decode("utf-8-sig" if expect_bom else "utf-8")
        except UnicodeDecodeError:
            errors.append(f"{label}: invalid UTF-8")
        if expect_crlf:
            if b"\n" in data.replace(b"\r\n", b"") or b"\r" in data.replace(b"\r\n", b""):
                errors.append(f"{label}: expected CRLF only")
        elif b"\r" in data:
            errors.append(f"{label}: expected LF only")

    for error in errors:
        print(error, file=sys.stderr)
    print(f"Checked text conventions in {checked} files; {len(errors)} issue(s).")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
