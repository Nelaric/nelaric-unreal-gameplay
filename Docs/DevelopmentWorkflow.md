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

GitHub Actions checks PR and commit naming, formatting and text conventions, and API documentation. The Linux build workflow first validates the PR and triggers CircleCI, then shows parallel Game, Editor, and Server jobs in the Actions run graph, followed by a result summary job. CircleCI builds the exact submitted commit with Unreal Engine 5.6.1 on Linux, whether the pull request comes from a fork or this repository. Each Actions job reports its target's result and links to the corresponding CircleCI log in its job summary. When the CircleCI job finishes, the Actions job also shows collapsible CircleCI step results and the checkout validation and compile commands and their console output. The complete log remains available in CircleCI; authentication and setup output is not copied to Actions. Game, Editor, and Server also appear as separate pull request checks: `ci/linux-game-build`, `ci/linux-editor-build`, and `ci/linux-server-build`. The existing `ci/fork-pr-linux-build` check summarizes the three jobs. You do not need a CircleCI account or any additional setup.

The three GitHub Actions checks and the CircleCI summary check remain required before merging to `main`. The three target checks show each build result independently.

For a pull request from a fork, the automated build does not accept changes to CI workflows, automation scripts, Unreal build scripts, or plugin descriptors. If your change needs one of these files, discuss it with a maintainer and use a branch in the source repository. Pull requests from branches in this repository can include those changes and still run the Linux build.

## What the Linux build covers

One CircleCI pipeline starts three independent Linux jobs at the same time. They build `NelaricGameplay/NelaricGameplay.uproject` with Unreal Engine 5.6.1, compiling the `NelaricGameplay` (Game), `NelaricGameplayEditor` (Editor), and `NelaricGameplayServer` (Server) targets in Development configuration. Each job checks out the same PR commit and uses its own workspace, so Unreal build outputs cannot collide. The enabled Core, Foundation, and PuerTS plugins are compiled as part of the project builds. PuerTS uses the V8 backend stored in the repository; these builds do not run Setup or install the editor TypeScript tooling. The workflow succeeds only when all three jobs succeed. It does not publish a downloadable package. Each job pulls the engine image without a build cache.

The project supports UE 5.6 and later, but CI currently tests only UE 5.6.1 on Linux. Testing the latest supported engine version remains a [build coverage gap](CodingStandards/BuildAndReview.md#unreal-build-scripts). There is no automated test coverage requirement yet.

## If a check fails

Open the failed target job in the GitHub Actions run to read its result summary and follow its CircleCI job log link. The [CircleCI pipeline](https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server) retains the project's original slug after the GitHub repository rename. If a check does not appear or you cannot resolve a failure, leave a comment on the pull request with the link and what you have tried.

The workflow definitions are available in [GitHub Actions Quality](../.github/workflows/quality.yml), [Fork PR Linux build](../.github/workflows/fork-pr-linux.yml), [API Pages](../.github/workflows/pages.yml), and [CircleCI](../.circleci/config.yml).
