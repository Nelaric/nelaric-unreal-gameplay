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
        post_status.assert_called_once_with("a" * 40, "pending", "Waiting for UE 5.6.1 Linux build")
        trigger.assert_called_once()
        monitor.assert_called_once()


if __name__ == "__main__":
    unittest.main()
