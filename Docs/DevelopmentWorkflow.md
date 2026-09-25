<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](DevelopmentWorkflow.zh-CN.md)

# Development and CI Workflow

This page explains the checks you will see on a pull request. For guidance on preparing a contribution, start with the [contribution guide](../CONTRIBUTING.md).

## From a change to `main`

1. Make a focused change on a branch or fork. Run the [local checks](CodingStandards/BuildAndReview.md#local-checks) relevant to your change.
2. Open a pull request to `main`. The automated checks start when you open or update it.
3. Review any failure, make a correction, and wait for the checks and maintainer review before merging.
4. After merge, the API documentation site is published from `main`. Validation and project target builds have already finished on the pull request.

## Checks on a pull request

GitHub Actions checks PR and commit naming, formatting and text conventions, and API documentation. CircleCI also builds the project at the exact submitted commit with Unreal Engine 5.6.1 on Linux, whether the pull request comes from a fork or this repository. The build result appears on the pull request as `ci/fork-pr-linux-build`. You do not need a CircleCI account or any additional setup.

All four check statuses are required before merging to `main`. They run on the pull request, before the change is merged.

For a pull request from a fork, the automated build does not accept changes to CI workflows, automation scripts, Unreal build scripts, or plugin descriptors. If your change needs one of these files, discuss it with a maintainer and use a branch in the source repository. Pull requests from branches in this repository can include those changes and still run the Linux build.

## What the Linux build covers

CircleCI builds `NelaricGameplay/NelaricGameplay.uproject` with Unreal Engine 5.6.1 on Linux. It compiles the `NelaricGameplay` (Game), `NelaricGameplayEditor` (Editor), and `NelaricGameplayServer` (Server) targets in Development configuration. The enabled `NelaricGameplayCore` plugin is compiled as part of the project builds. The job does not publish a downloadable package. By design, it downloads the engine image on every run and does not use a build cache.

The project supports UE 5.6 and later, but CI currently tests only UE 5.6.1 on Linux. Testing the latest supported engine version remains a [build coverage gap](CodingStandards/BuildAndReview.md#unreal-build-scripts). There is no automated test coverage requirement yet.

## If a check fails

Open the failed check from the pull request. The Linux build status links to the matching [CircleCI pipeline](https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server), which retains the project's original slug after the GitHub repository rename. There you can see which build step failed. If a check does not appear or you cannot resolve a failure, leave a comment on the pull request with the link and what you have tried.

The workflow definitions are available in [GitHub Actions Quality](../.github/workflows/quality.yml), [Fork PR Linux build](../.github/workflows/fork-pr-linux.yml), [API Pages](../.github/workflows/pages.yml), and [CircleCI](../.circleci/config.yml).
