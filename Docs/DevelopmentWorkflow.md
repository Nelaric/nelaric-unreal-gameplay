<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](DevelopmentWorkflow.zh-CN.md)

# Development and CI Workflow

## From change to `main`

1. Make a focused change on a branch or fork and run the relevant [local checks](CodingStandards/BuildAndReview.md#local-checks).
2. Open a pull request to `main`. GitHub Actions runs the **Quality** workflow. A trusted GitHub Actions workflow checks the PR target, exact head commit, and changed files; for eligible fork PRs it starts the CircleCI GitHub App Linux build and reports the result to that PR commit. The required checks and review must finish before merging.
3. After a change reaches `main`, GitHub Actions reruns the format and API documentation checks and the **API Pages** workflow publishes the documentation site. The CircleCI GitHub App pipeline compiles the plugin again for the `main` push.

See the [contribution guide](../CONTRIBUTING.md) for branch, pull request, and review requirements. The workflow definitions are [GitHub Actions Quality](../.github/workflows/quality.yml), [Fork PR Linux build](../.github/workflows/fork-pr-linux.yml), [API Pages](../.github/workflows/pages.yml), and [CircleCI](../.circleci/config.yml).

## Pull request quality checks

The GitHub Actions **Quality** workflow runs on fork pull requests without project secrets. Its required checks cover formatting and text conventions, public API documentation, and PR and commit naming. Contributors do not need a CircleCI project or project credentials.

The `Protect main` ruleset requires those three GitHub Actions quality checks and `ci/fork-pr-linux-build` from GitHub Actions.

## Linux build before merging a PR

The `Fork PR Linux build` GitHub Actions workflow runs from the trusted base branch using `pull_request_target`. It never checks out or executes PR files on the GitHub runner. For fork PRs, it verifies that the open PR targets `Nelaric/nelaric-unreal-server:main`, pins its current head SHA, and rejects changes to `.circleci/`, GitHub Actions workflows and automation, `Scripts/`, Unreal build scripts, plugin descriptors, `.gitattributes`, and `.gitmodules`. To change these protected files, ask a maintainer to handle the change in a branch of the source repository; that branch receives a successful not-applicable fork-build status.

For an eligible PR, the workflow sends its number and SHA through a project-specific CircleCI custom webhook. The GitHub App pipeline loads its configuration and initial checkout from `main`, verifies the PR and SHA again, fetches `refs/pull/<number>/head`, and checks out that exact commit. It then copies only the plugin into an isolated UE container without network access or the GHCR credentials. The GitHub workflow waits for the CircleCI result and posts `ci/fork-pr-linux-build` to the PR head SHA. CircleCI's own App status belongs to the initial `main` checkout, so it is not the PR merge gate.

The custom webhook URL is stored as the GitHub repository secret `CIRCLECI_PR_BUILD_WEBHOOK_URL`. Its CircleCI GitHub App trigger uses `main` for both configuration and checkout. The GitHub workflow needs only `contents: read`, `pull-requests: read`, and `statuses: write`; no separate bot account is required. Fork PR #21 verified the build and GitHub status reporting before `ci/fork-pr-linux-build` became a required `main` ruleset status with GitHub Actions as its expected source. CircleCI project pipeline data is public and the bot reads its API to monitor the matching run.

## CircleCI Linux plugin build

The CircleCI GitHub App pipeline runs `build_plugin_linux` for pushes to `main` and `build_fork_pr_linux` for the trusted custom webhook. Both use a Linux Large machine, pull Epic's `ghcr.io/epicgames/unreal-engine:dev-slim-5.6.1` image, and run Unreal Automation Tool `BuildPlugin` for `Unreal-Plugins/NelaricServer/NelaricServer.uplugin` with `-TargetPlatforms=Linux`.

This checks that the source-only plugin compiles and packages for Linux with UE 5.6.1. It does not build a game or a Dedicated Server target. The package is written to temporary storage inside the job and is not published as a downloadable artifact. The job does not enable Docker layer caching or save a CircleCI cache, so it pulls the engine image on each run.

The project supports UE 5.6 and later, but these jobs currently test only UE 5.6.1 on Linux. The [build standard](CodingStandards/BuildAndReview.md#unreal-build-scripts) recommends compiling both the minimum and latest supported releases; the latest release remains a coverage gap. There is no automated test coverage requirement at this stage.

## Credentials and troubleshooting

The CircleCI project stores `GHCR_USERNAME` and `GHCR_TOKEN` as environment variables. `GHCR_USERNAME` is the GitHub account allowed to pull Epic's engine image; `GHCR_TOKEN` is a classic personal access token with only `read:packages`. Keep the token out of the repository, pull requests, and logs. The trusted PR build can use these variables only to pull the image before entering the isolated plugin build container; never pass them to that container. Replace the token in CircleCI if it expires or is revoked.

Find build results in the [CircleCI project](https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server). For a manual run, select the GitHub App pipeline and choose `main` for both configuration and checkout sources. If the image pull fails, check GHCR access and the two project environment variables. If compilation fails, inspect the `Compile and package NelaricServer for Linux` step and reproduce with the same engine version.
