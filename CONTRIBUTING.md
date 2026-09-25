<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](CONTRIBUTING.zh-CN.md)

# Contributing to Nelaric Unreal Gameplay

Thank you for helping build Nelaric Unreal Gameplay. Reports, design feedback, documentation improvements, and code contributions are welcome in English or Chinese.

The `NelaricGameplay/` directory contains the framework's Unreal project. Describe the developer problem and intended gameplay workflow when proposing a capability.

## Before opening an issue

Search [existing issues](https://github.com/Nelaric/nelaric-unreal-gameplay/issues) and read the [project overview](README.md) and [coding standards](Docs/CodingStandards/). Use the [issue chooser](https://github.com/Nelaric/nelaric-unreal-gameplay/issues/new/choose) to select the form that fits:

- **Bug report:** Include reproduction steps, expected and actual behavior, and relevant environment details. If Unreal Engine is involved, include its version, operating system, and whether the problem occurs in standalone play, a listen server, a dedicated server, or a client.
- **Capability proposal:** Explain the developer problem and use case, desired behavior, and where the responsibility should live. A public API sketch is useful but optional.
- **Documentation issue:** Link the page or section and explain what is unclear, incorrect, or missing.

Blank issues remain available for topics that do not fit these forms. Keep each issue focused on one problem or proposal, and leave private information out of public reports.

## Project scope

The framework targets Unreal Engine 5.6 and later. It provides shared gameplay contracts across standalone, listen-server, and dedicated-server topologies. Foundation supports persistent worlds and bounded activities; optional modules provide match flow, objectives, and scoring. Games supply concrete rules and content. Unreal Engine networking continues to handle gameplay state replication. Versioned cooked content delivery is a separate capability from gameplay execution.

When proposing a change, identify whether it belongs in Foundation, an optional integration, content-update tooling, development tooling, or the game. Genre-specific rules and presentation remain with the game or optional modules. Explain how a Foundation addition applies to more than one gameplay lifecycle.

## Before opening a pull request

Before contributing code, we recommend opening an issue to discuss whether the change is needed, how it fits the project, and its proposed scope. Reaching agreement early makes the resulting pull request easier to review and more likely to be accepted. Small fixes and documentation corrections may still go directly to a pull request.

If you are not a member of this repository, fork it first. Create a branch in your fork, then open a pull request from that branch to `Nelaric/nelaric-unreal-gameplay:main`.

Keep a pull request focused. Use the [pull request template](.github/pull_request_template.md) to describe the problem, the chosen approach, any public API or integration impact, and how you verified the change. Link a related issue or capability proposal when one exists; use a closing keyword only when the pull request fully resolves it. Use `None` for design and compatibility impact on documentation-only changes, for risks or exceptions when there are none, and for reviewer notes when no specific guidance is needed. Otherwise, point reviewers to the files, decisions, questions, or tradeoffs that need attention. If you request an exception to a project rule, name the rule, affected scope, reason, and alternatives considered.

All code contributions must follow [Epic Games' Unreal Engine C++ Coding Standard](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) and the [project coding standards](Docs/CodingStandards/README.md). The project standards also define text encoding, local checks, and the allowed PR title, branch, and commit prefixes. Use a branch name such as `docs/clarify-contribution-guide` and a PR title or commit subject such as `docs: clarify contribution guide`.

Run the checks relevant to your change as described in [Build Scripts, Tooling, and Review](Docs/CodingStandards/BuildAndReview.md). In your pull request, say which checks you ran and mention any you could not run.

GitHub Actions checks naming, formatting, and API documentation. CircleCI also compiles the submitted project's Game, Editor, and Server targets with Unreal Engine 5.6.1 on Linux before merge, whether the PR comes from a fork or a branch in this repository. You do not need a CircleCI account or any additional setup. If a fork PR changes CI workflows, automation, Unreal build scripts, or plugin descriptors, discuss it with a maintainer; these changes need a branch in the source repository. See the [development and CI workflow](Docs/DevelopmentWorkflow.md) for the check sequence and build scope. CI does not enforce test coverage.

Pull requests target `main`. Repository protection requires passing checks and review before merging. Maintainers review correctness, module boundaries, API contracts, performance, security, and compatibility with Unreal Engine 5.6 and later.
