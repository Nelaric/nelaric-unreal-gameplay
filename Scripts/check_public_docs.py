#!/usr/bin/env python3
# Copyright (c) 2026 Nelaric
"""Check public header file comments and first-public gameplay methods."""

from __future__ import annotations

import re
import sys

from check_text import ROOT, repository_files

DOC_COMMENT = re.compile(r"/\*\*(.*?)\*/", re.DOTALL)
FILE_TAG = re.compile(r"@file\b")
NON_CODE = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.DOTALL)
CLASS_BODY = re.compile(r"\b(?:class|struct)\b[^;{}]*\{", re.DOTALL)
ACCESS = re.compile(r"(?m)^[ \t]*(?:public|protected|private):[ \t]*$")
CALL = re.compile(r"\b([A-Za-z_~]\w*)\s*\(")
NON_METHOD_CALLS = {"UFUNCTION", "UPROPERTY", "GENERATED_BODY", "GENERATED_UCLASS_BODY", "GENERATED_USTRUCT_BODY"}
DECLARATION_PREFIX = re.compile(r"[\w:\s<>,*&\[\]]+")


def code_only(source: str) -> str:
    return NON_CODE.sub(lambda match: re.sub(r"[^\n]", " ", match.group()), source)


def closing_brace(code: str, opening: int) -> int:
    depth = 1
    for position in range(opening + 1, len(code)):
        if code[position] == "{":
            depth += 1
        elif code[position] == "}":
            depth -= 1
            if depth == 0:
                return position
    return len(code)


def has_preceding_doxygen(lines: list[str], line_index: int) -> bool:
    previous = line_index - 1
    while previous >= 0:
        line = lines[previous].strip()
        # clang-format may put export macros and return types before the name.
        prefix = DECLARATION_PREFIX.fullmatch(line) and line not in {"public:", "protected:", "private:"}
        if line and not line.startswith("UFUNCTION(") and not prefix:
            break
        previous -= 1
    if previous < 0:
        return False
    if lines[previous].lstrip().startswith("///"):
        return True
    if not lines[previous].rstrip().endswith("*/"):
        return False
    while previous >= 0:
        if "/**" in lines[previous]:
            return True
        if "/*" in lines[previous]:
            return False
        previous -= 1
    return False


def undocumented_first_public_methods(source: str) -> list[tuple[int, str]]:
    code = code_only(source)
    lines = source.splitlines()
    missing = []
    for class_match in CLASS_BODY.finditer(code):
        opening = class_match.end() - 1
        end = closing_brace(code, opening)
        sections = [
            match for match in ACCESS.finditer(code, opening + 1, end)
            if code.count("{", opening, match.start()) - code.count("}", opening, match.start()) == 1
        ]
        first_public = next((index for index, match in enumerate(sections) if match.group().strip() == "public:"), None)
        if first_public is None:
            continue
        start = sections[first_public].end()
        stop = sections[first_public + 1].start() if first_public + 1 < len(sections) else end
        position = start
        for line in code[start:stop].splitlines(keepends=True):
            match = CALL.search(line)
            if match and match.group(1) not in NON_METHOD_CALLS and not line.lstrip().startswith("#"):
                absolute = position + match.start()
                if code.count("{", opening, absolute) - code.count("}", opening, absolute) == 1:
                    line_index = source.count("\n", 0, absolute)
                    if not has_preceding_doxygen(lines, line_index):
                        missing.append((line_index + 1, match.group(1)))
            position += len(line)
    return missing


def access_between_public_sections(source: str) -> list[tuple[int, str]]:
    code = code_only(source)
    invalid = []
    for class_match in CLASS_BODY.finditer(code):
        opening = class_match.end() - 1
        end = closing_brace(code, opening)
        sections = [
            match for match in ACCESS.finditer(code, opening + 1, end)
            if code.count("{", opening, match.start()) - code.count("}", opening, match.start()) == 1
        ]
        public_sections = [index for index, match in enumerate(sections) if match.group().strip() == "public:"]
        if len(public_sections) < 2:
            continue
        for section in sections[public_sections[0] + 1:public_sections[1]]:
            invalid.append((source.count("\n", 0, section.start()) + 1, section.group().strip()))
    return invalid


def has_file_comment(source: str) -> bool:
    first_comment = DOC_COMMENT.search(source)
    if first_comment is None or FILE_TAG.search(first_comment.group(1)) is None:
        return False
    pragma = source.find("#pragma once")
    return pragma < 0 or first_comment.start() < pragma


def public_headers():
    return [
        path
        for path in repository_files()
        if path.is_file() and path.suffix.lower() in {".h", ".hpp"} and "Public" in path.relative_to(ROOT).parts
    ]


def main() -> int:
    headers = public_headers()
    issues = []
    for path in headers:
        source = path.read_text(encoding="utf-8-sig")
        label = path.relative_to(ROOT).as_posix()
        if not has_file_comment(source):
            issues.append(f"{label}: missing Doxygen @file comment")
        for line, specifier in access_between_public_sections(source):
            issues.append(f"{label}:{line}: {specifier} appears between the first and second public: sections")
        for line, method in undocumented_first_public_methods(source):
            issues.append(f"{label}:{line}: first-public method {method} lacks Doxygen")
    for issue in issues:
        print(issue, file=sys.stderr)
    print(f"Checked {len(headers)} public headers; {len(issues)} issue(s).")
    return 1 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
