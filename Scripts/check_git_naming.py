# Copyright (c) 2026 Nelaric
"""Validate pull request and commit names against the project standard."""

from __future__ import annotations

import json
import os
import re
import sys
from urllib.parse import quote
from urllib.request import Request, urlopen

PREFIXES = (
    "feat",
    "fix",
    "docs",
    "style",
    "refactor",
    "perf",
    "test",
    "build",
    "ci",
    "chore",
    "revert",
)
PREFIX_PATTERN = "(?:" + "|".join(PREFIXES) + ")"
SUBJECT_PATTERN = re.compile(
    rf"{PREFIX_PATTERN}(?:\([a-z][a-z0-9-]*\))?: [a-z][\x20-\x7e]*"
)
BRANCH_PATTERN = re.compile(rf"{PREFIX_PATTERN}/[a-z0-9]+(?:-[a-z0-9]+)*")


def validate(title: str, branch: str, commits: list[str]) -> list[str]:
    """Return human-readable violations for one pull request."""
    errors = []
    if SUBJECT_PATTERN.fullmatch(title) is None:
        errors.append(f"PR title: {title!r}")
    if BRANCH_PATTERN.fullmatch(branch) is None:
        errors.append(f"Source branch: {branch!r}")
    if not commits:
        errors.append("PR has no commits to check")
    for index, subject in enumerate(commits, start=1):
        if SUBJECT_PATTERN.fullmatch(subject) is None:
            errors.append(f"Commit {index}: {subject!r}")
    return errors


def github_json(url: str, token: str) -> object:
    request = Request(
        url,
        headers={
            "Accept": "application/vnd.github+json",
            "Authorization": f"Bearer {token}",
            "User-Agent": "nelaric-git-naming-check",
        },
    )
    with urlopen(request, timeout=30) as response:
        return json.load(response)


def pull_request_commits(pull_url: str, token: str) -> list[str]:
    """Read PR commits through GitHub's paginated REST endpoint."""
    subjects = []
    page = 1
    while True:
        commits = github_json(f"{pull_url}/commits?per_page=100&page={page}", token)
        subjects.extend(commit["commit"]["message"].splitlines()[0] for commit in commits)
        if len(commits) < 100:
            return subjects
        page += 1


def main() -> int:
    match = re.fullmatch(r"refs/pull/([1-9][0-9]*)/(?:merge|head)", os.environ["GITHUB_REF"])
    if match is None:
        print(f"Expected a pull request ref, got {os.environ['GITHUB_REF']!r}", file=sys.stderr)
        return 1
    pull_url = (
        f"{os.environ['GITHUB_API_URL']}/repos/"
        f"{quote(os.environ['GITHUB_REPOSITORY'], safe='/')}/pulls/{match.group(1)}"
    )
    token = os.environ["GITHUB_TOKEN"]
    pull_request = github_json(pull_url, token)
    commits = pull_request_commits(pull_url, token)
    if len(commits) != pull_request["commits"]:
        print(
            f"Expected {pull_request['commits']} PR commit(s), but the GitHub API returned "
            f"{len(commits)}. Re-run the check after the PR updates; GitHub's PR commits "
            "endpoint cannot list more than 250 commits.",
            file=sys.stderr,
        )
        return 1
    errors = validate(pull_request["title"], pull_request["head"]["ref"], commits)
    if errors:
        print("Naming check failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        print(
            "Expected PR and commit title: <prefix>: <lowercase English summary> "
            "or <prefix>(<lowercase-scope>): <lowercase English summary>.",
            file=sys.stderr,
        )
        print(
            "Expected branch: <prefix>/<lowercase-kebab-case-description>. "
            f"Allowed prefixes: {', '.join(PREFIXES)}.",
            file=sys.stderr,
        )
        return 1
    print(f"Naming check passed for PR title, branch, and {len(commits)} commit(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
