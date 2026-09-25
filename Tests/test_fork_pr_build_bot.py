# Copyright (c) 2026 Nelaric

import os
import sys
import unittest
from pathlib import Path
from unittest.mock import patch


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "Scripts"))

import fork_pr_build_bot


class ForkPrBuildBotTests(unittest.TestCase):
    def test_in_repository_pr_starts_linux_build(self) -> None:
        pull = {"head": {"sha": "a" * 40, "repo": {"full_name": "Nelaric/nelaric-unreal-gameplay"}}}
        with (
            patch.dict(os.environ, {"PR_NUMBER": "23", "GITHUB_TOKEN": "test-token"}),
            patch.object(fork_pr_build_bot, "get_pull_request", return_value=pull),
            patch.object(fork_pr_build_bot, "verify_pull_request") as verify,
            patch.object(fork_pr_build_bot, "post_status") as post_status,
            patch.object(fork_pr_build_bot, "trigger_circleci") as trigger,
            patch.object(fork_pr_build_bot, "monitor") as monitor,
        ):
            fork_pr_build_bot.main()

        verify.assert_called_once_with(23, "a" * 40, "test-token")
        self.assertEqual(4, post_status.call_count)
        post_status.assert_any_call("a" * 40, "pending", "Waiting for UE 5.6.1 Linux build")
        for context in fork_pr_build_bot._JOB_CONTEXTS.values():
            post_status.assert_any_call(
                "a" * 40, "pending", "Waiting for UE 5.6.1 Linux build", None, context=context
            )
        trigger.assert_called_once()
        monitor.assert_called_once()

    def test_reports_each_finished_job_as_a_separate_check(self) -> None:
        sha = "a" * 40
        pipeline = {"id": "pipeline-id", "number": 42}
        workflow = {"id": "workflow-id", "status": "failed"}
        jobs = {
            name: {
                "name": name,
                "status": "failed" if name == "build_server_linux" else "success",
                "job_number": index,
            }
            for index, name in enumerate(fork_pr_build_bot._JOB_CONTEXTS, start=1)
        }
        with (
            patch.dict(os.environ, {"GITHUB_TOKEN": "test-token"}),
            patch.object(fork_pr_build_bot, "get_pull_request", return_value={"head": {"sha": sha}}),
            patch.object(fork_pr_build_bot, "find_pipeline", return_value=pipeline),
            patch.object(fork_pr_build_bot, "find_workflow", return_value=workflow),
            patch.object(fork_pr_build_bot, "workflow_jobs", return_value=jobs),
            patch.object(fork_pr_build_bot, "post_status") as post_status,
        ):
            self.assertFalse(fork_pr_build_bot.monitor(23, sha, "nonce"))

        for name, context in fork_pr_build_bot._JOB_CONTEXTS.items():
            state = "failure" if name == "build_server_linux" else "success"
            self.assertTrue(
                any(
                    item.kwargs.get("context") == context and item.args[1] == state
                    for item in post_status.call_args_list
                )
            )
        self.assertEqual("failure", post_status.call_args.args[1])

    def test_missing_job_cannot_pass_the_build(self) -> None:
        sha = "a" * 40
        with (
            patch.dict(os.environ, {"GITHUB_TOKEN": "test-token"}),
            patch.object(fork_pr_build_bot, "get_pull_request", return_value={"head": {"sha": sha}}),
            patch.object(fork_pr_build_bot, "find_pipeline", return_value={"id": "pipeline-id", "number": 42}),
            patch.object(fork_pr_build_bot, "find_workflow", return_value={"id": "workflow-id", "status": "success"}),
            patch.object(fork_pr_build_bot, "workflow_jobs", return_value={}),
            patch.object(fork_pr_build_bot, "post_status") as post_status,
        ):
            self.assertFalse(fork_pr_build_bot.monitor(23, sha, "nonce"))
        self.assertEqual(["error", "error", "error", "failure"], [item.args[1] for item in post_status.call_args_list])

    def test_reports_finished_job_while_other_jobs_are_running(self) -> None:
        sha = "a" * 40
        names = list(fork_pr_build_bot._JOB_CONTEXTS)
        first = {names[0]: {"name": names[0], "status": "success", "job_number": 1}}
        second = {
            name: {"name": name, "status": "success", "job_number": index}
            for index, name in enumerate(names, start=1)
        }
        with (
            patch.dict(os.environ, {"GITHUB_TOKEN": "test-token"}),
            patch.object(fork_pr_build_bot, "get_pull_request", return_value={"head": {"sha": sha}}),
            patch.object(fork_pr_build_bot, "find_pipeline", return_value={"id": "pipeline-id", "number": 42}),
            patch.object(
                fork_pr_build_bot,
                "find_workflow",
                side_effect=[{"id": "workflow-id", "status": "running"}, {"id": "workflow-id", "status": "success"}],
            ),
            patch.object(fork_pr_build_bot, "workflow_jobs", side_effect=[first, second]),
            patch.object(fork_pr_build_bot, "post_status") as post_status,
            patch.object(fork_pr_build_bot.time, "sleep"),
        ):
            self.assertTrue(fork_pr_build_bot.monitor(23, sha, "nonce"))
        self.assertEqual(4, post_status.call_count)
        self.assertEqual("success", post_status.call_args.args[1])

    def test_reads_all_workflow_job_pages(self) -> None:
        pages = [
            {"items": [{"name": "build_game_linux", "status": "success"}], "next_page_token": "next"},
            {"items": [{"name": "build_server_linux", "status": "failed"}], "next_page_token": None},
        ]
        with patch.object(fork_pr_build_bot, "request_json", side_effect=pages) as request_json:
            jobs = fork_pr_build_bot.workflow_jobs("workflow-id")
        self.assertEqual({"build_game_linux", "build_server_linux"}, set(jobs))
        self.assertIn("page-token=next", request_json.call_args.args[0])


if __name__ == "__main__":
    unittest.main()
