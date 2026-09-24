# Copyright (c) 2026 Nelaric
"""Validate a pull request before a privileged CircleCI build."""

from __future__ import annotations

import json
import os
import re
import sys
import urllib.parse
import urllib.request


REPOSITORY = "Nelaric/nelaric-unreal-server"
_API_ROOT = f"https://api.github.com/repos/{REPOSITORY}"
_SHA_PATTERN = re.compile(r"[0-9a-f]{40}\Z")
_PROTECTED_PREFIXES = (
    ".circleci/",
    ".github/actions/",
    ".github/scripts/",
    ".github/workflows/",
    "scripts/",
)
_PROTECTED_SUFFIXES = (".build.cs", ".target.cs", ".uplugin", ".uproject")


class PolicyError(Exception):
    """The pull request is ineligible for an authenticated build."""


def is_protected_path(path: str) -> bool:
    lower = path.lower()
    return (
        lower in {".gitattributes", ".gitmodules", "dockerfile"}
        or lower.startswith(_PROTECTED_PREFIXES)
        or lower.endswith(_PROTECTED_SUFFIXES)
    )


def github_json(path: str, token: str | None = None) -> object:
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "nelaric-pr-build-policy",
    }
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(f"{_API_ROOT}{path}", headers=headers)
    with urllib.request.urlopen(request, timeout=30) as response:
        return json.load(response)


def get_pull_request(number: int, token: str | None = None) -> dict:
    if number < 1:
        raise PolicyError("Invalid pull request number")
    result = github_json(f"/pulls/{number}", token)
    if not isinstance(result, dict):
        raise PolicyError("Invalid GitHub pull request response")
    return result


def changed_paths(number: int, token: str | None = None) -> list[str]:
    paths: list[str] = []
    for page in range(1, 31):
        query = urllib.parse.urlencode({"per_page": 100, "page": page})
        result = github_json(f"/pulls/{number}/files?{query}", token)
        if not isinstance(result, list):
            raise PolicyError("Invalid GitHub pull request files response")
        for item in result:
            paths.append(item["filename"])
            if item.get("previous_filename"):
                paths.append(item["previous_filename"])
        if len(result) < 100:
            return paths
    raise PolicyError("Pull request has too many files to inspect safely")


def verify_pull_request(number: int, expected_sha: str, token: str | None = None) -> None:
    if not _SHA_PATTERN.fullmatch(expected_sha):
        raise PolicyError("Invalid pull request commit SHA")
    pull = get_pull_request(number, token)
    base = pull.get("base") or {}
    head = pull.get("head") or {}
    base_repository = base.get("repo") or {}
    if pull.get("state") != "open":
        raise PolicyError("Pull request is not open")
    if base_repository.get("full_name") != REPOSITORY or base.get("ref") != "main":
        raise PolicyError("Pull request must target Nelaric/nelaric-unreal-server:main")
    if head.get("sha") != expected_sha:
        raise PolicyError("Pull request changed after the build was requested")
    protected = [path for path in changed_paths(number, token) if is_protected_path(path)]
    if protected:
        raise PolicyError("Protected CI or build files changed: " + ", ".join(protected[:5]))


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("Usage: pr_build_policy.py PR_NUMBER HEAD_SHA")
    number = int(sys.argv[1])
    sha = sys.argv[2]
    verify_pull_request(number, sha, os.environ.get("GITHUB_TOKEN"))
    print(f"{number} {sha}")


if __name__ == "__main__":
    main()
