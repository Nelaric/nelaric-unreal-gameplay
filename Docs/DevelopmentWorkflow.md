<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](DevelopmentWorkflow.zh-CN.md)

# Development and CI Workflow

This page explains the checks you will see on a pull request. For guidance on preparing a contribution, start with the [contribution guide](../CONTRIBUTING.md).

## From a change to `main`

1. Make a focused change on a branch or fork. Run the [local checks](CodingStandards/BuildAndReview.md#local-checks) relevant to your change.
2. Open a pull request to `main`. The automated checks start when you open or update it.
3. Review any failure, make a correction, and wait for the checks and maintainer review before merging.
4. After merge, the plugin is built again on Linux and the API documentation site is updated.

## Checks on a pull request

GitHub Actions checks PR and commit naming, formatting and text conventions, and API documentation. For pull requests from forks, CircleCI also builds the exact submitted plugin commit with Unreal Engine 5.6.1 on Linux. The build result appears on the pull request as `ci/fork-pr-linux-build`. You do not need a CircleCI account or any additional setup.

All four check statuses are required before merging to `main`. A pull request from a branch in this repository receives a passing fork-build status without a separate fork build; the plugin is built again after the change reaches `main`.

The automated fork build does not accept changes to CI workflows, automation scripts, Unreal build scripts, or plugin descriptors. If your change needs one of these files, discuss it with a maintainer and use a branch in the source repository.

## What the Linux build covers

CircleCI compiles and packages `Unreal-Plugins/NelaricServer/NelaricServer.uplugin` for Linux with Unreal Engine 5.6.1. This verifies the source-only plugin. It does not build a game or a Dedicated Server target, and it does not publish the package as a downloadable artifact. The engine image is downloaded for each run; the job does not use a build cache.

The project supports UE 5.6 and later, but CI currently tests only UE 5.6.1 on Linux. Testing the latest supported engine version remains a [build coverage gap](CodingStandards/BuildAndReview.md#unreal-build-scripts). There is no automated test coverage requirement yet.

## If a check fails

Open the failed check from the pull request. The Linux build status links to the matching [CircleCI pipeline](https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server), where you can see which build step failed. If a check does not appear or you cannot resolve a failure, leave a comment on the pull request with the link and what you have tried.

The workflow definitions are available in [GitHub Actions Quality](../.github/workflows/quality.yml), [Fork PR Linux build](../.github/workflows/fork-pr-linux.yml), [API Pages](../.github/workflows/pages.yml), and [CircleCI](../.circleci/config.yml).
