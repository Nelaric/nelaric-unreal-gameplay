# Copyright (c) 2026 Nelaric
"""Behavioral checks for authenticated fork PR build policy."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Scripts"))

from fork_pr_build_bot import find_pipeline  # noqa: E402
from pr_build_policy import PolicyError, changed_paths, is_protected_path, verify_pull_request  # noqa: E402


SHA = "a" * 40


class PullRequestBuildPolicyTests(unittest.TestCase):
    def setUp(self) -> None:
        self.pull = {
            "state": "open",
            "base": {"ref": "main", "repo": {"full_name": "Nelaric/nelaric-unreal-server"}},
            "head": {"sha": SHA},
        }

    def test_rejects_changes_that_can_control_ci_or_build(self) -> None:
        for path in (
            ".circleci/config.yml",
            ".github/workflows/quality.yml",
            "Scripts/pr_build_policy.py",
            "Unreal-Plugins/NelaricServer/Source/NelaricCore/NelaricCore.Build.cs",
            "Unreal-Plugins/NelaricServer/NelaricServer.uplugin",
        ):
            with self.subTest(path=path):
                self.assertTrue(is_protected_path(path))
        self.assertFalse(is_protected_path("Unreal-Plugins/NelaricServer/Source/NelaricCore/Private/Core.cpp"))

    def test_accepts_only_the_current_commit_of_an_open_main_pr(self) -> None:
        with (
            patch("pr_build_policy.get_pull_request", return_value=self.pull),
            patch("pr_build_policy.changed_paths", return_value=["README.md"]),
        ):
            verify_pull_request(19, SHA)
            with self.assertRaisesRegex(PolicyError, "changed"):
                verify_pull_request(19, "b" * 40)

    def test_rejects_protected_files(self) -> None:
        with (
            patch("pr_build_policy.get_pull_request", return_value=self.pull),
            patch("pr_build_policy.changed_paths", return_value=[".circleci/config.yml"]),
        ):
            with self.assertRaisesRegex(PolicyError, "Protected"):
                verify_pull_request(19, SHA)

    def test_renaming_a_protected_file_still_blocks_the_build(self) -> None:
        files = [{"filename": "old-config.txt", "previous_filename": ".circleci/config.yml"}]
        with patch("pr_build_policy.github_json", return_value=files):
            self.assertIn(".circleci/config.yml", changed_paths(19))

    def test_matches_only_the_requested_circleci_run(self) -> None:
        response = {
            "items": [
                {"id": "other", "trigger_parameters": {"webhook": {"body": '{"nonce":"wrong"}'}}},
                {"id": "wanted", "trigger_parameters": {"webhook": {"body": '{"nonce":"match"}'}}},
            ]
        }
        with patch("fork_pr_build_bot.request_json", return_value=response):
            self.assertEqual("wanted", find_pipeline("match")["id"])


if __name__ == "__main__":
    unittest.main()
