<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](CONTRIBUTING.zh-CN.md)

# Contributing to Nelaric Unreal Server

Thank you for helping build Nelaric Unreal Server. Reports, design feedback, documentation improvements, and code contributions are welcome in English or Chinese.

The repository contains a loadable plugin scaffold, but no server services or public API yet. The local validation project under `Development/` is excluded from Git. Please describe a proposed capability as a proposal, rather than assuming its API or implementation already exists.

## Before opening an issue

Search [existing issues](https://github.com/Nelaric/nelaric-unreal-server/issues) and read the [project overview](README.md) and [coding standards](Docs/CodingStandards/). Use the [issue chooser](https://github.com/Nelaric/nelaric-unreal-server/issues/new/choose) to select the form that fits:

- **Bug report:** Include what exists today, reproduction steps, expected and actual behavior, and relevant environment details. If Unreal Engine is involved, include its version, operating system, and whether the problem occurs in the Editor, client, or Dedicated Server.
- **Capability proposal:** Explain the developer problem and use case, desired behavior, and where the responsibility should live. A public API sketch is useful but optional.
- **Documentation issue:** Link the page or section and explain what is unclear, incorrect, or missing.

Blank issues remain available for topics that do not fit these forms. Keep each issue focused on one problem or proposal, and leave private information out of public reports.

## Project scope

The framework targets Unreal Engine 5.6 and later. It supplies gameplay-agnostic Dedicated Server infrastructure such as lifecycle, admission, sessions, generic persistence, backend communication, observability, and hosting integration. Core defines mechanisms; games and optional providers choose their own policies and implementations. Unreal Engine networking continues to handle high-frequency gameplay state.

When proposing a change, identify whether it belongs in Core, a provider, development tooling, or the game. Inventory, combat, character rules, and matchmaking algorithms are examples of game-specific behavior outside Core.

## Before opening a pull request

Before contributing code, we recommend opening an issue to discuss whether the change is needed, how it fits the project, and its proposed scope. Reaching agreement early makes the resulting pull request easier to review and more likely to be accepted. Small fixes and documentation corrections may still go directly to a pull request.

If you are not a member of this repository, fork it first. Create a branch in your fork, then open a pull request from that branch to `Nelaric/nelaric-unreal-server:main`.

Keep a pull request focused. Use the [pull request template](.github/pull_request_template.md) to describe the problem, the chosen approach, any public API or provider impact, and how you verified the change. Link a related issue or capability proposal when one exists; use a closing keyword only when the pull request fully resolves it. Use `None` for design and compatibility impact on documentation-only changes, for risks or exceptions when there are none, and for reviewer notes when no specific guidance is needed. Otherwise, point reviewers to the files, decisions, questions, or tradeoffs that need attention. If you request an exception to a project rule, name the rule, affected scope, reason, and alternatives considered.

All code contributions must follow [Epic Games' Unreal Engine C++ Coding Standard](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) and the [project coding standards](Docs/CodingStandards/README.md). The project standards also define text encoding, local checks, and the allowed PR title, branch, and commit prefixes. Use a branch name such as `docs/clarify-contribution-guide` and a PR title or commit subject such as `docs: clarify contribution guide`.

Run the checks relevant to your change as described in [Build Scripts, Tooling, and Review](Docs/CodingStandards/BuildAndReview.md). In your pull request, say which checks you ran and mention any you could not run.

GitHub Actions checks naming, formatting, and API documentation. For a pull request from a fork, CircleCI also compiles the submitted plugin commit with Unreal Engine 5.6.1 on Linux before merge. You do not need a CircleCI account or any additional setup. If your change affects CI workflows, automation, Unreal build scripts, or plugin descriptors, discuss it with a maintainer; the automated fork build does not accept changes to those files. See the [development and CI workflow](Docs/DevelopmentWorkflow.md) for the check sequence and build scope. CI does not enforce test coverage.

Pull requests target `main`. Repository protection requires passing checks and review before merging. Maintainers review correctness, module boundaries, API contracts, performance, security, and compatibility with Unreal Engine 5.6 and later.
