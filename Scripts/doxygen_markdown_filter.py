#!/usr/bin/env python3
# Copyright (c) 2026 Nelaric
"""Resolve source-relative Markdown navigation for Doxygen's root-based links."""

from __future__ import annotations

import sys
from pathlib import Path


def main() -> int:
    source = Path(sys.argv[1])
    content = source.read_text(encoding="utf-8-sig")
    if source.name in {"README.md", "README.zh-CN.md"} and source.parent.name in {"Api", "CodingStandards"}:
        target = "README.zh-CN.md" if source.name == "README.md" else "README.md"
        content = content.replace(f"]({target})", f"](Docs/{source.parent.name}/{target})", 1)
    if source.name == "README.md" and source.parent.name == "CodingStandards":
        content = content.replace(
            "](BuildAndReview.md#copyright-notices)",
            "](Docs/CodingStandards/BuildAndReview.md)",
            1,
        )
    sys.stdout.write(content)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
