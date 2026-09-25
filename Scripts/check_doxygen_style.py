#!/usr/bin/env python3
# Copyright (c) 2026 Nelaric
"""Check mechanical style rules for project-authored C++ Doxygen comments."""

from __future__ import annotations

import re
import sys

from check_text import ROOT, repository_files

MAX_CONTENT_LENGTH = 75
MAX_TRAILING_LENGTH = 25
CXX_SUFFIXES = {".h", ".hpp", ".cpp", ".cc", ".cxx"}
BLOCK_START = re.compile(r"/\*\*")
LEADING_COMMENT = re.compile(r"^[ \t]*///(?!<)(.*)$")
OTHER_DOXYGEN_STYLE = re.compile(r"^[ \t]*(?://!|/\*!)")
ENUM_DECLARATION = re.compile(r"^[ \t]*enum\b")
DOXYGEN_TAG = re.compile(r"@[A-Za-z][A-Za-z0-9_]*\b")
BACKSLASH_TAG = re.compile(
    r"\\(?:file|brief|details|par|param|tparam|return|pre|post|note|warning|see|deprecated)\b"
)


def comment_content(line: str, *, first: bool) -> str:
    """Remove indentation and one comment marker and its separating space."""
    content = line.lstrip(" \t")
    if not first and content.startswith("*"):
        content = content[1:]
    if content.startswith(" "):
        content = content[1:]
    return content.rstrip(" \t")


def check_source(source: str, label: str) -> list[str]:
    issues: list[str] = []
    previous_leading = False
    enum_pending = False
    enum_depth = 0
    enum_comment_column: int | None = None
    enum_has_leading_comment = False
    enum_trailing_lines: list[int] = []
    for line, raw_line in enumerate(source.splitlines(), start=1):
        if enum_depth == 0 and ENUM_DECLARATION.match(raw_line):
            enum_pending = ";" not in raw_line or "{" in raw_line
        enum_opened = enum_pending and "{" in raw_line
        if enum_opened:
            enum_pending = False
            enum_comment_column = None
            enum_has_leading_comment = False
            enum_trailing_lines = []
        in_enum = enum_depth > 0 or enum_opened
        if OTHER_DOXYGEN_STYLE.match(raw_line):
            issues.append(f"{label}:{line}: use ///, ///<, or /** ... */")
        if "///<" in raw_line:
            prefix, raw_content = raw_line.split("///<", 1)
            content = raw_content.removeprefix(" ").rstrip(" \t")
            if in_enum:
                enum_trailing_lines.append(line)
                if not prefix.strip():
                    issues.append(f"{label}:{line}: place ///< after an enum value")
                column = len(prefix.expandtabs(4))
                if enum_comment_column is None:
                    enum_comment_column = column
                elif column != enum_comment_column:
                    issues.append(
                        f"{label}:{line}: align the first / of enum ///< comments"
                    )
            elif not prefix.rstrip().endswith(";"):
                issues.append(f"{label}:{line}: place ///< after a variable")
            if not content or len(content) > MAX_TRAILING_LENGTH:
                issues.append(
                    f"{label}:{line}: ///< content must be 1 to "
                    f"{MAX_TRAILING_LENGTH} characters"
                )
        leading = LEADING_COMMENT.match(raw_line)
        if in_enum and (leading or raw_line.lstrip(" \t").startswith("/**")):
            enum_has_leading_comment = True
        if leading:
            content = leading.group(1).removeprefix(" ").rstrip(" \t")
            if not content or len(content) > MAX_CONTENT_LENGTH:
                issues.append(
                    f"{label}:{line}: /// content must be 1 to "
                    f"{MAX_CONTENT_LENGTH} characters"
                )
            if previous_leading:
                issues.append(
                    f"{label}:{line}: use a tagged /** ... */ block for multiple lines"
                )
        previous_leading = leading is not None
        if in_enum:
            enum_depth += raw_line.count("{") - raw_line.count("}")
            if enum_depth <= 0:
                if enum_has_leading_comment:
                    for trailing_line in enum_trailing_lines:
                        issues.append(
                            f"{label}:{trailing_line}: use /// before every enum value "
                            "when the enum needs a leading comment"
                        )
                enum_depth = 0
                enum_comment_column = None

    cursor = 0
    while match := BLOCK_START.search(source, cursor):
        start = match.end()
        end = source.find("*/", start)
        line = source.count("\n", 0, match.start()) + 1
        line_start = source.rfind("\n", 0, match.start()) + 1
        if source[line_start:match.start()].strip():
            issues.append(f"{label}:{line}: put Doxygen comments before declarations")
        if end < 0:
            issues.append(f"{label}:{line}: unterminated Doxygen comment")
            break
        body = source[start:end]
        if "\n" not in body and not re.match(r"\s*@file\b", body):
            issues.append(f"{label}:{line}: use /// for a single-line declaration comment")
        has_tag = False
        paragraph_start = True
        for offset, raw_line in enumerate(body.split("\n")):
            content = comment_content(raw_line, first=offset == 0)
            current_line = line + offset
            if offset and content and not raw_line.lstrip(" \t").startswith("*"):
                issues.append(f"{label}:{current_line}: prefix continued lines with *")
            if len(content) > MAX_CONTENT_LENGTH:
                issues.append(
                    f"{label}:{current_line}: Doxygen content is {len(content)} "
                    f"characters (maximum {MAX_CONTENT_LENGTH})"
                )
            if BACKSLASH_TAG.search(content):
                issues.append(f"{label}:{current_line}: use @ for Doxygen tags")
            if not content:
                paragraph_start = True
                continue
            if paragraph_start and DOXYGEN_TAG.match(content) is None:
                issues.append(
                    f"{label}:{current_line}: start each paragraph with a Doxygen @tag"
                )
            paragraph_start = False
            for tag in DOXYGEN_TAG.finditer(content):
                if tag.start() != 0:
                    issues.append(
                        f"{label}:{current_line}: put Doxygen tags on their own line"
                    )
                else:
                    has_tag = True
        if not has_tag:
            issues.append(f"{label}:{line}: /** ... */ requires a Doxygen @tag")
        cursor = end + 2
    return issues


def project_cpp_files():
    for path in repository_files():
        if not path.is_file() or path.suffix.lower() not in CXX_SUFFIXES:
            continue
        parts = path.relative_to(ROOT).parts
        if parts[0] == "NelaricGameplay" and "Source" in parts:
            yield path


def main() -> int:
    paths = list(project_cpp_files())
    issues: list[str] = []
    for path in paths:
        label = path.relative_to(ROOT).as_posix()
        issues.extend(check_source(path.read_text(encoding="utf-8-sig"), label))
    for issue in issues:
        print(issue, file=sys.stderr)
    print(f"Checked Doxygen style in {len(paths)} C++ files; {len(issues)} issue(s).")
    return 1 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
