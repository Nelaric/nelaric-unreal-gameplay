# Copyright (c) 2026 Nelaric Contributors
"""Behavioral checks for authenticated fork PR build policy."""

from __future__ import annotations

import sys
import http.client
import io
import urllib.error
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Scripts"))

from fork_pr_build_bot import find_pipeline  # noqa: E402
from pr_build_policy import PolicyError, changed_paths, github_json, is_protected_path, verify_pull_request  # noqa: E402


SHA = "a" * 40


class PullRequestBuildPolicyTests(unittest.TestCase):
    def test_retries_disconnected_pr_read_before_returning_current_head(self) -> None:
        response = io.BytesIO(b'{"head":{"sha":"current-head"}}')
        with (
            patch("pr_build_policy.urllib.request.urlopen", side_effect=[http.client.RemoteDisconnected(), response]) as read,
            patch("pr_build_policy.time.sleep") as sleep,
        ):
            self.assertEqual("current-head", github_json("/pulls/70")["head"]["sha"])
        self.assertEqual(2, read.call_count)
        sleep.assert_called_once_with(2)

    def test_stops_retrying_disconnected_pr_reads(self) -> None:
        with (
            patch("pr_build_policy.urllib.request.urlopen", side_effect=http.client.RemoteDisconnected()) as read,
            patch("pr_build_policy.time.sleep") as sleep,
        ):
            with self.assertRaises(http.client.RemoteDisconnected):
                github_json("/pulls/70")
        self.assertEqual(5, read.call_count)
        self.assertEqual([2, 4, 8, 16], [call.args[0] for call in sleep.call_args_list])

    def test_retries_transient_http_errors_but_preserves_authorization_failures(self) -> None:
        for status in (429, 500, 502, 503, 504, 401, 403):
            with self.subTest(status=status):
                error = urllib.error.HTTPError("https://api.github.test", status, "error", {}, None)
                with (
                    patch("pr_build_policy.urllib.request.urlopen", side_effect=error) as read,
                    patch("pr_build_policy.time.sleep") as sleep,
                ):
                    with self.assertRaises(urllib.error.HTTPError):
                        github_json("/pulls/70")
                transient = status in {429, 500, 502, 503, 504}
                self.assertEqual(5 if transient else 1, read.call_count)
                self.assertEqual(4 if transient else 0, sleep.call_count)

    def setUp(self) -> None:
        self.pull = {
            "state": "open",
            "base": {"ref": "main", "repo": {"full_name": "Nelaric/nelaric-unreal-gameplay"}},
            "head": {"sha": SHA, "repo": {"full_name": "LKZ2022/nelaric-unreal-gameplay"}},
        }

    def test_rejects_changes_that_can_control_ci_or_build(self) -> None:
        for path in (
            ".circleci/config.yml",
            ".github/workflows/quality.yml",
            "Scripts/pr_build_policy.py",
            "NelaricGameplay/Plugins/NelaricGameplay/Source/GameplayRuntime/GameplayRuntime.Build.cs",
            "NelaricGameplay/Plugins/NelaricGameplay/NelaricGameplay.uplugin",
        ):
            with self.subTest(path=path):
                self.assertTrue(is_protected_path(path))
        self.assertFalse(is_protected_path("NelaricGameplay/Plugins/NelaricGameplay/Source/GameplayRuntime/Private/Core.cpp"))

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

    def test_allows_in_repository_pr_to_build_with_ci_changes(self) -> None:
        self.pull["head"]["repo"]["full_name"] = "Nelaric/nelaric-unreal-gameplay"
        with (
            patch("pr_build_policy.get_pull_request", return_value=self.pull),
            patch("pr_build_policy.changed_paths") as changed_paths_mock,
        ):
            verify_pull_request(19, SHA)
            changed_paths_mock.assert_not_called()

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
