# Copyright (c) 2026 Nelaric
"""Check the result of the project's Unreal automation run."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def check_report(report_path: Path, log_path: Path, editor_status: int) -> None:
    if not report_path.is_file():
        if log_path.is_file():
            log = log_path.read_text(encoding="utf-8", errors="replace")
            if "No automation tests matched 'StartsWith:Nelaric'" in log and "**** TEST COMPLETE." in log:
                print("No Nelaric automation tests were registered")
                return
        raise ValueError("Automation report is missing")

    report = json.loads(report_path.read_text(encoding="utf-8-sig"))
    if not isinstance(report, dict):
        raise ValueError("Automation report must be a JSON object")
    succeeded = report.get("succeeded")
    failed = report.get("failed")
    warnings = report.get("succeededWithWarnings")
    not_run = report.get("notRun")
    in_process = report.get("inProcess", 0)
    if any(not isinstance(value, int) or value < 0 for value in (succeeded, failed, warnings, not_run, in_process)):
        raise ValueError("Automation report has invalid test counts")
    print(
        f"Automation results: {succeeded} succeeded, {warnings} with warnings, "
        f"{failed} failed, {not_run} not run, {in_process} in progress"
    )
    if failed or not_run or in_process:
        raise ValueError("Automation tests failed or did not finish")
    if editor_status:
        raise ValueError(f"Unreal Editor exited with status {editor_status}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        raise SystemExit("Usage: check_automation_report.py REPORT_JSON EDITOR_LOG EDITOR_STATUS")
    check_report(Path(sys.argv[1]), Path(sys.argv[2]), int(sys.argv[3]))
