# Copyright (c) 2026 Nelaric
"""Behavioral checks for pull request naming validation."""

from __future__ import annotations

import sys
import unittest
from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Scripts"))

from check_git_naming import PREFIXES, main, pull_request_commits, validate  # noqa: E402


class GitNamingTests(unittest.TestCase):
    def test_every_allowed_prefix(self) -> None:
        for prefix in PREFIXES:
            with self.subTest(prefix=prefix):
                self.assertEqual(
                    [],
                    validate(
                        f"{prefix}: add naming check",
                        f"{prefix}/add-naming-check",
                        [f"{prefix}(ci): add naming check"],
                    ),
                )

    def test_rejects_invalid_title_branch_and_every_invalid_commit(self) -> None:
        errors = validate(
            "Docs: Add bilingual documentation",
            "docs/Add_bilingual_documentation",
            ["docs: add documentation", "unknown: add feature", "fix: 修复问题"],
        )
        self.assertEqual(4, len(errors))
        self.assertTrue(errors[0].startswith("PR title:"))
        self.assertTrue(errors[1].startswith("Source branch:"))
        self.assertTrue(errors[2].startswith("Commit 2:"))
        self.assertTrue(errors[3].startswith("Commit 3:"))

    def test_rejects_missing_commits_and_invalid_scope(self) -> None:
        self.assertIn("PR has no commits to check", validate("ci: add check", "ci/add-check", []))
        self.assertTrue(validate("ci(CI): add check", "ci/add-check", ["ci: add check"]))

    def test_reads_all_commit_pages(self) -> None:
        first_page = [{"commit": {"message": "ci: add check\n\nDetails"}}] * 100
        second_page = [{"commit": {"message": "test: verify pagination"}}]
        with patch("check_git_naming.github_json", side_effect=[first_page, second_page]) as get:
            subjects = pull_request_commits("https://api.github.test/pulls/9", "test-token")
        self.assertEqual(101, len(subjects))
        self.assertEqual("ci: add check", subjects[0])
        self.assertEqual("test: verify pagination", subjects[-1])
        self.assertEqual(2, get.call_count)

    def test_rejects_incomplete_api_results(self) -> None:
        environment = {
            "GITHUB_REF": "refs/pull/9/merge",
            "GITHUB_API_URL": "https://api.github.test",
            "GITHUB_REPOSITORY": "Nelaric/nelaric-unreal-gameplay",
            "GITHUB_TOKEN": "test-token",
        }
        pull_request = {
            "title": "ci: add naming check",
            "head": {"ref": "ci/add-naming-check"},
            "commits": 251,
        }
        with (
            patch.dict("os.environ", environment),
            patch("check_git_naming.github_json", return_value=pull_request),
            patch("check_git_naming.pull_request_commits", return_value=["ci: add check"] * 250),
            redirect_stderr(StringIO()) as output,
        ):
            result = main()
        self.assertEqual(1, result)
        self.assertIn("Expected 251 PR commit(s)", output.getvalue())


if __name__ == "__main__":
    unittest.main()
