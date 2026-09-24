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


def post_status(sha: str, state: str, description: str, target_url: str | None = None) -> None:
    payload = {"state": state, "context": _STATUS_CONTEXT, "description": description[:140]}
    if target_url:
        payload["target_url"] = target_url
    request_json(
        f"https://api.github.com/repos/{_REPOSITORY}/statuses/{sha}",
        token=os.environ["GITHUB_TOKEN"],
        payload=payload,
    )


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


def workflow_status(pipeline_id: str) -> str | None:
    result = request_json(f"https://circleci.com/api/v2/pipeline/{pipeline_id}/workflow")
    matching = [item for item in result.get("items", []) if item.get("name") == "fork_pr_linux_build"]
    if not matching:
        return None
    return matching[0].get("status")


def monitor(number: int, sha: str, nonce: str) -> None:
    deadline = time.monotonic() + 55 * 60
    pipeline: dict | None = None
    while time.monotonic() < deadline:
        if get_pull_request(number, os.environ["GITHUB_TOKEN"])["head"]["sha"] != sha:
            post_status(sha, "error", "Superseded by a newer pull request commit")
            return
        if pipeline is None:
            pipeline = find_pipeline(nonce)
        if pipeline is not None:
            target_url = (
                "https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server/"
                f"{pipeline['number']}/details"
            )
            status = workflow_status(pipeline["id"])
            if status in _FINAL_STATES:
                state = "success" if status == "success" else "failure"
                post_status(sha, state, f"UE 5.6.1 Linux build: {status}", target_url)
                if state != "success":
                    raise RuntimeError(f"CircleCI workflow finished with status {status}")
                return
            if pipeline.get("errors"):
                post_status(sha, "error", "CircleCI could not start the PR build", target_url)
                raise RuntimeError("CircleCI pipeline has configuration errors")
        time.sleep(15)
    post_status(sha, "error", "Timed out waiting for the CircleCI PR build")
    raise RuntimeError("Timed out waiting for CircleCI")


def main() -> None:
    number = int(os.environ["PR_NUMBER"])
    token = os.environ["GITHUB_TOKEN"]
    pull = get_pull_request(number, token)
    sha = pull["head"]["sha"]
    try:
        verify_pull_request(number, sha, token)
    except PolicyError as error:
        post_status(sha, "failure", str(error))
        raise
    post_status(sha, "pending", "Waiting for UE 5.6.1 Linux build")
    nonce = str(uuid.uuid4())
    try:
        trigger_circleci(number, sha, nonce)
        monitor(number, sha, nonce)
    except Exception:
        # Never print the webhook URL, which contains a trigger secret.
        if find_pipeline(nonce) is None:
            post_status(sha, "error", "Could not start the CircleCI PR build")
        raise


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"Fork PR build bot failed: {type(error).__name__}: {error}", file=sys.stderr)
        raise SystemExit(1) from None
