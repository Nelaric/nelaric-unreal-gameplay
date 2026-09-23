<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](DevelopmentWorkflow.zh-CN.md)

# Development and CI Workflow

## From change to `main`

1. Make a focused change on a branch or fork and run the relevant [local checks](CodingStandards/BuildAndReview.md#local-checks).
2. Open a pull request to `main`. GitHub Actions runs the **Quality** workflow: PR and commit naming, format and text conventions, and API documentation. The CircleCI GitHub OAuth pipeline runs the secretless `public_quality` job for fork pull requests. The repository requires these checks and a review before merging.
3. After a change reaches `main`, GitHub Actions reruns the format and API documentation checks and the **API Pages** workflow publishes the documentation site. The separate CircleCI GitHub App pipeline runs the Linux plugin build. The Unreal compilation runs after merge; it is not a required pull request check.

See the [contribution guide](../CONTRIBUTING.md) for branch, pull request, and review requirements. The workflow definitions are [GitHub Actions Quality](../.github/workflows/quality.yml), [API Pages](../.github/workflows/pages.yml), and [CircleCI](../.circleci/config.yml).

## CircleCI fork pull request check

The GitHub OAuth pipeline builds fork pull requests when **Build forked pull requests** is enabled in CircleCI project Advanced settings. Its `public_quality` job checks out the commit with a repository-specific read-only deploy key and runs the text and public API documentation checks. It does not pull Unreal Engine, compile the plugin, or use a restricted context. Keep **Pass secrets to builds from forked pull requests** disabled. Contributors do not need a CircleCI project or project credentials.

The OAuth pipeline reports `ci/circleci: public_quality` to GitHub. The `Protect main` ruleset requires that status from CircleCI App, alongside the three GitHub Actions quality checks. The GitHub App Unreal build remains a post-merge check because GitHub App pipelines do not trigger for fork pull requests. If no OAuth status appears, check the OAuth trigger, fork-build setting, GitHub webhook, and checkout access before editing branch protection.

## CircleCI Linux plugin build

The CircleCI GitHub App trigger and workflow are limited to pushes to `main`. The `build_plugin_linux` job uses a Linux Large machine, pulls Epic's `ghcr.io/epicgames/unreal-engine:dev-slim-5.6.1` image, and runs Unreal Automation Tool `BuildPlugin` for `Unreal-Plugins/NelaricServer/NelaricServer.uplugin` with `-TargetPlatforms=Linux`.

This checks that the source-only plugin compiles and packages for Linux with UE 5.6.1. It does not build a game or a Dedicated Server target. The package is written to temporary storage inside the job and is not published as a downloadable artifact. The job does not enable Docker layer caching or save a CircleCI cache, so it pulls the engine image on each run.

The project supports UE 5.6 and later, but this job currently tests only UE 5.6.1 on Linux. The [build standard](CodingStandards/BuildAndReview.md#unreal-build-scripts) recommends compiling both the minimum and latest supported releases; the latest release remains a coverage gap. There is no automated test coverage requirement at this stage.

## Credentials and troubleshooting

The CircleCI project stores `GHCR_USERNAME` and `GHCR_TOKEN` as environment variables. `GHCR_USERNAME` is the GitHub account allowed to pull Epic's engine image; `GHCR_TOKEN` is a classic personal access token with only `read:packages`. Keep the token out of the repository, pull requests, and logs. Fork builds must never receive these variables. Replace the token in CircleCI if it expires or is revoked.

Find build results in the [CircleCI project](https://app.circleci.com/pipelines/github/Nelaric/nelaric-unreal-server). For a manual run, select the GitHub App pipeline and choose `main` for both configuration and checkout sources. If the image pull fails, check GHCR access and the two project environment variables. If compilation fails, inspect the `Compile and package NelaricServer for Linux` step and reproduce with the same engine version.
