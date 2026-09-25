<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](README.zh-CN.md)

# Project Coding Standards

These standards apply to every contribution to Nelaric Unreal Gameplay. Contributors must follow both [Epic Games' Unreal Engine C++ Coding Standard](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) and these project coding standards. If their requirements conflict, report the conflict in an issue before proceeding; neither standard takes precedence automatically. Formatting is determined by the pinned tools only when their output complies with both standards.

The supported baseline is Unreal Engine 5.6 and later. The project is a multi-module gameplay framework for standalone, listen-server, and dedicated-server play. Foundation owns shared gameplay contracts; optional modules provide specific mechanisms. Unreal Engine's NetDriver, replication, Iris, and RPC systems continue to handle gameplay networking.

## Standards

- [C++ style and header hygiene](Cpp.md)
- [Modules and dependency boundaries](Modules.md)
- [Public API, errors, and documentation](Api.md)
- [Doxygen comments](Doxygen.md)
- [Runtime, concurrency, performance, and security](Runtime.md)
- [Build scripts, tooling, and review](BuildAndReview.md)

The [copyright notice rule](BuildAndReview.md#copyright-notices) applies to every project-authored file, subject to format-specific exceptions.

**Must** is mandatory. **Should** is the default unless a PR explains a justified exception. **May** is optional. Public API means declarations intended for use outside their owning module, including gameplay extension contracts and Blueprint-exposed entry points.

## Compliance and conflicts

1. Correctness and Unreal Engine 5.6+ requirements apply first.
2. Follow both Epic and project requirements. If they appear inconsistent, open an issue identifying the exact rules and wait for the standards to be reconciled rather than choosing one rule over the other.
3. Pinned formatter output is authoritative for whitespace and layout only where it satisfies both standards. Report a conflicting formatter result in an issue.
4. A PR requesting an exception to a project guideline must identify the rule, reason, scope, and alternatives considered. A maintainer must approve it. Such an exception cannot override an Epic requirement or resolve a conflict between the two standards by itself.

This standard does not prescribe a project-wide choice between plain C++, UObject, and reflected types. Each module chooses the appropriate form and documents its lifetime and thread behavior.
