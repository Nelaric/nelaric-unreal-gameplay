# Copyright (c) 2026 Nelaric
"""Trigger a trusted CircleCI build and report its result to the PR commit."""

from __future__ import annotations

import json
import os
import sys
import time
import urllib.parse
import urllib.request
import uuid

from pr_build_policy import PolicyError, get_pull_request, verify_pull_request


_REPOSITORY = "Nelaric/nelaric-unreal-gameplay"
# CircleCI retains the original project slug after the GitHub repository rename.
_CIRCLECI_PROJECT = "gh/Nelaric/nelaric-unreal-server"
_STATUS_CONTEXT = "ci/fork-pr-linux-build"
_JOB_CONTEXTS = {
    "build_game_linux": "ci/linux-game-build",
    "build_editor_linux": "ci/linux-editor-build",
    "build_server_linux": "ci/linux-server-build",
}
_JOB_LABELS = {
    "build_game_linux": "Game",
    "build_editor_linux": "Editor",
    "build_server_linux": "Server",
}
_FINAL_STATES = {"success", "failed", "error", "canceled", "unauthorized", "not_run"}


def request_json(url: str, *, token: str | None = None, payload: dict | None = None) -> object:
    headers = {"Accept": "application/json", "User-Agent": "nelaric-fork-pr-build-bot"}
    if token:
        headers["Authorization"] = f"Bearer {token}"
    if payload is not None:
        headers["Content-Type"] = "application/json"
    data = json.dumps(payload).encode() if payload is not None else None
    request = urllib.request.Request(url, data=data, headers=headers)
    with urllib.request.urlopen(request, timeout=30) as response:
        body = response.read()
    if not body:
        return {}
    try:
        return json.loads(body)
    except ValueError:
        return {}


def post_status(
    sha: str, state: str, description: str, target_url: str | None = None, *, context: str = _STATUS_CONTEXT
) -> None:
    payload = {"state": state, "context": context, "description": description[:140]}
    if target_url:
        payload["target_url"] = target_url
    request_json(
        f"https://api.github.com/repos/{_REPOSITORY}/statuses/{sha}",
        token=os.environ["GITHUB_TOKEN"],
        payload=payload,
    )


def post_job_statuses(sha: str, state: str, description: str, target_url: str | None = None) -> None:
    for context in _JOB_CONTEXTS.values():
        post_status(sha, state, description, target_url, context=context)


def trigger_circleci(number: int, sha: str, nonce: str) -> None:
    webhook = os.environ.get("CIRCLECI_PR_BUILD_WEBHOOK_URL", "")
    parsed = urllib.parse.urlparse(webhook)
    if parsed.scheme != "https" or parsed.hostname != "internal.circleci.com":
        raise RuntimeError("CircleCI PR build webhook secret is missing or invalid")
    try:
        request_json(webhook, payload={"pr": number, "sha": sha, "nonce": nonce})
    except OSError:
        raise RuntimeError("CircleCI trigger request failed") from None


def find_pipeline(nonce: str) -> dict | None:
    base = f"https://circleci.com/api/v2/project/{_CIRCLECI_PROJECT}/pipeline"
    page_token: str | None = None
    for _ in range(5):
        query = {"branch": "main"}
        if page_token:
            query["page-token"] = page_token
        result = request_json(f"{base}?{urllib.parse.urlencode(query)}")
        for pipeline in result.get("items", []):
            raw_body = pipeline.get("trigger_parameters", {}).get("webhook", {}).get("body", "{}")
            try:
                body = json.loads(raw_body) if isinstance(raw_body, str) else raw_body
            except (ValueError, TypeError):
                continue
            if isinstance(body, dict) and body.get("nonce") == nonce:
                return pipeline
        page_token = result.get("next_page_token")
        if not page_token:
            break
    return None


def find_workflow(pipeline_id: str) -> dict | None:
    result = request_json(f"https://circleci.com/api/v2/pipeline/{pipeline_id}/workflow")
    matching = [item for item in result.get("items", []) if item.get("name") == "fork_pr_linux_build"]
    if not matching:
        return None
    return matching[0]


def workflow_jobs(workflow_id: str) -> dict[str, dict]:
    jobs: dict[str, dict] = {}
    page_token: str | None = None
    while True:
        query = f"?{urllib.parse.urlencode({'page-token': page_token})}" if page_token else ""
        result = request_json(f"https://circleci.com/api/v2/workflow/{workflow_id}/job{query}")
        for job in result.get("items", []):
            if job.get("name") in _JOB_CONTEXTS:
                jobs[job["name"]] = job
        page_token = result.get("next_page_token")
        if not page_token:
            return jobs


def monitor(number: int, sha: str, nonce: str) -> bool:
    deadline = time.monotonic() + 55 * 60
    pipeline: dict | None = None
    reported: set[str] = set()

    def fail_pending(description: str, target_url: str | None = None) -> None:
        for name, context in _JOB_CONTEXTS.items():
            if name not in reported:
                post_status(sha, "error", description, target_url, context=context)

    while time.monotonic() < deadline:
        if get_pull_request(number, os.environ["GITHUB_TOKEN"])["head"]["sha"] != sha:
            post_status(sha, "error", "Superseded by a newer pull request commit")
            fail_pending("Superseded by a newer pull request commit")
            return False
        if pipeline is None:
            pipeline = find_pipeline(nonce)
        if pipeline is not None:
            target_url = (
                "https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server/"
                f"{pipeline['number']}/details"
            )
            workflow = find_workflow(pipeline["id"])
            if workflow:
                jobs = workflow_jobs(workflow["id"])
                for name, context in _JOB_CONTEXTS.items():
                    if name in reported:
                        continue
                    job = jobs.get(name)
                    job_status = job.get("status") if job else None
                    if job_status not in _FINAL_STATES:
                        if job is None and workflow.get("status") in _FINAL_STATES:
                            post_status(sha, "error", f"UE 5.6.1 Linux {name}: missing", target_url, context=context)
                            reported.add(name)
                        continue
                    state = "success" if job_status == "success" else "failure"
                    job_url = target_url
                    if job.get("job_number"):
                        job_url = (
                            "https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server/"
                            f"{pipeline['number']}/workflows/{workflow['id']}/jobs/{job['job_number']}"
                        )
                    post_status(sha, state, f"UE 5.6.1 Linux {name}: {job_status}", job_url, context=context)
                    reported.add(name)
                if workflow.get("status") in _FINAL_STATES and len(reported) == len(_JOB_CONTEXTS):
                    all_succeeded = workflow["status"] == "success" and all(
                        jobs.get(name, {}).get("status") == "success" for name in _JOB_CONTEXTS
                    )
                    state = "success" if all_succeeded else "failure"
                    post_status(sha, state, f"UE 5.6.1 Linux build: {workflow['status']}", target_url)
                    return all_succeeded
            if pipeline.get("errors"):
                post_status(sha, "error", "CircleCI could not start the PR build", target_url)
                fail_pending("CircleCI could not start the PR build", target_url)
                return False
        time.sleep(15)
    post_status(sha, "error", "Timed out waiting for the CircleCI PR build")
    fail_pending("Timed out waiting for the CircleCI PR build")
    return False


def write_summary(lines: list[str]) -> None:
    path = os.environ.get("GITHUB_STEP_SUMMARY")
    if path:
        with open(path, "a", encoding="utf-8") as summary:
            summary.write("\n".join(lines) + "\n")


def start_main() -> None:
    number = int(os.environ["PR_NUMBER"])
    token = os.environ["GITHUB_TOKEN"]
    sha = get_pull_request(number, token)["head"]["sha"]
    try:
        verify_pull_request(number, sha, token)
        post_status(sha, "pending", "Waiting for UE 5.6.1 Linux builds")
        post_job_statuses(sha, "pending", "Waiting for UE 5.6.1 Linux build")
        nonce = str(uuid.uuid4())
        trigger_circleci(number, sha, nonce)
    except Exception:
        post_status(sha, "error", "Could not start the CircleCI PR build")
        post_job_statuses(sha, "error", "Could not start the CircleCI PR build")
        write_summary(["## CircleCI build start", "PR validation or pipeline trigger failed. See this job's log."])
        raise
    with open(os.environ["GITHUB_OUTPUT"], "a", encoding="utf-8") as output:
        output.write(f"sha={sha}\nnonce={nonce}\n")
    write_summary(
        [
            "## CircleCI build start",
            f"- Pull request: #{number}",
            f"- Commit: `{sha}`",
            "- Engine: Unreal Engine 5.6.1 on Linux",
            "- Targets: Game, Editor, Server in three parallel CircleCI jobs",
        ]
    )


def watch_target(number: int, sha: str, nonce: str, job_name: str) -> tuple[str, str, str | None]:
    deadline = time.monotonic() + 55 * 60
    pipeline: dict | None = None
    last_status: str | None = None
    while time.monotonic() < deadline:
        if get_pull_request(number, os.environ["GITHUB_TOKEN"])["head"]["sha"] != sha:
            return "error", "Superseded by a newer pull request commit", None
        if pipeline is None:
            pipeline = find_pipeline(nonce)
            if pipeline:
                print(f"Found CircleCI pipeline #{pipeline['number']} for {_JOB_LABELS[job_name]}", flush=True)
        if pipeline:
            pipeline_url = (
                "https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server/"
                f"{pipeline['number']}/details"
            )
            if pipeline.get("errors"):
                return "error", "CircleCI could not start the PR build", pipeline_url
            workflow = find_workflow(pipeline["id"])
            if workflow:
                job = workflow_jobs(workflow["id"]).get(job_name)
                job_status = job.get("status") if job else "waiting"
                if job_status != last_status:
                    print(f"{_JOB_LABELS[job_name]}: {job_status}", flush=True)
                    last_status = job_status
                if job and job.get("status") in _FINAL_STATES:
                    status = job["status"]
                    job_url = pipeline_url
                    if job.get("job_number"):
                        job_url = (
                            "https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server/"
                            f"{pipeline['number']}/workflows/{workflow['id']}/jobs/{job['job_number']}"
                        )
                    return "success" if status == "success" else "failure", status, job_url
                if workflow.get("status") in _FINAL_STATES and job is None:
                    return "error", "CircleCI job was not created", pipeline_url
        time.sleep(15)
    return "error", "Timed out waiting for the CircleCI job", None


def watch_main() -> None:
    number = int(os.environ["PR_NUMBER"])
    sha = os.environ["PR_SHA"]
    nonce = os.environ["BUILD_NONCE"]
    job_name = os.environ["TARGET_JOB"]
    if job_name not in _JOB_CONTEXTS:
        raise ValueError("Unknown CircleCI target job")
    label = _JOB_LABELS[job_name]
    print(f"Tracking {label} for pull request #{number} at {sha}", flush=True)
    try:
        state, detail, target_url = watch_target(number, sha, nonce, job_name)
    except Exception:
        state, detail, target_url = "error", "Could not read the CircleCI job result", None
        post_status(sha, state, detail, context=_JOB_CONTEXTS[job_name])
        write_summary([f"## {label} Linux build", f"- Result: {detail}"])
        raise
    post_status(sha, state, f"UE 5.6.1 Linux {job_name}: {detail}", target_url, context=_JOB_CONTEXTS[job_name])
    lines = [
        f"## {label} Linux build",
        f"- Result: **{detail}**",
        "- Engine: Unreal Engine 5.6.1",
        "- Platform and configuration: Linux Development",
        f"- Commit: `{sha}`",
    ]
    if target_url:
        lines.append(f"- [CircleCI job log]({target_url})")
    write_summary(lines)
    print(f"{job_name}: {detail}", flush=True)
    if state != "success":
        raise RuntimeError(f"CircleCI {job_name} finished with {detail}")


def finish_main() -> None:
    sha = os.environ.get("PR_SHA", "")
    nonce = os.environ.get("BUILD_NONCE", "")
    start_result = os.environ.get("START_RESULT", "")
    build_result = os.environ.get("BUILD_RESULT", "")
    lines = ["## Linux build results", "| Target | CircleCI result |", "| --- | --- |"]
    all_succeeded = start_result == "success" and build_result == "success"
    if sha and nonce:
        try:
            if get_pull_request(int(os.environ["PR_NUMBER"]), os.environ["GITHUB_TOKEN"])["head"]["sha"] != sha:
                raise RuntimeError("Pull request was updated during the build")
            pipeline = find_pipeline(nonce)
            workflow = find_workflow(pipeline["id"]) if pipeline else None
            jobs = workflow_jobs(workflow["id"]) if workflow else {}
            for name in _JOB_CONTEXTS:
                job = jobs.get(name, {})
                status = job.get("status", "missing")
                result = status
                if pipeline and workflow and job.get("job_number"):
                    job_url = (
                        "https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server/"
                        f"{pipeline['number']}/workflows/{workflow['id']}/jobs/{job['job_number']}"
                    )
                    result = f"[{status}]({job_url})"
                lines.append(f"| {_JOB_LABELS[name]} | {result} |")
                all_succeeded &= status == "success"
            all_succeeded &= workflow is not None and workflow.get("status") == "success"
        except RuntimeError as error:
            all_succeeded = False
            lines.append(f"| Pull request | {error} |")
        except Exception:
            all_succeeded = False
            lines.append("| CircleCI API | Could not read final job results |")
        state = "success" if all_succeeded else "failure"
        post_status(sha, state, f"UE 5.6.1 Linux builds: {state}")
    else:
        lines.append("| Pipeline start | Failed before a commit was handed to the build jobs |")
    lines.append(f"\nActions build matrix: **{build_result or 'not started'}**")
    write_summary(lines)
    if not all_succeeded:
        raise RuntimeError("One or more Linux builds did not succeed")


def main() -> None:
    number = int(os.environ["PR_NUMBER"])
    token = os.environ["GITHUB_TOKEN"]
    pull = get_pull_request(number, token)
    sha = pull["head"]["sha"]
    try:
        verify_pull_request(number, sha, token)
    except PolicyError as error:
        post_status(sha, "failure", str(error))
        post_job_statuses(sha, "failure", str(error))
        raise
    post_status(sha, "pending", "Waiting for UE 5.6.1 Linux build")
    post_job_statuses(sha, "pending", "Waiting for UE 5.6.1 Linux build")
    nonce = str(uuid.uuid4())
    try:
        trigger_circleci(number, sha, nonce)
        succeeded = monitor(number, sha, nonce)
    except Exception:
        # Never print the webhook URL, which contains a trigger secret.
        post_status(sha, "error", "Could not complete the CircleCI PR build")
        post_job_statuses(sha, "error", "Could not complete the CircleCI PR build")
        raise
    if not succeeded:
        raise RuntimeError("CircleCI PR build did not succeed")


if __name__ == "__main__":
    try:
        modes = {"start": start_main, "watch": watch_main, "finish": finish_main}
        if len(sys.argv) == 1:
            main()
        elif len(sys.argv) == 2 and sys.argv[1] in modes:
            modes[sys.argv[1]]()
        else:
            raise ValueError("Usage: fork_pr_build_bot.py [start|watch|finish]")
    except Exception as error:
        print(f"Fork PR build bot failed: {type(error).__name__}: {error}", file=sys.stderr)
        raise SystemExit(1) from None
