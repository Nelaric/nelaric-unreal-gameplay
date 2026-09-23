<!-- Copyright (c) 2026 Nelaric -->

# Project Coding Standards

These standards apply to every contribution to Nelaric Unreal Server. Follow [Epic Games' Unreal Engine C++ Coding Standard](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) by default. An explicit rule here takes precedence over an Epic rule only where the two conflict; the project rule must state why the exception exists. Formatting is determined by the pinned tools, not by interpreting prose.

The supported baseline is Unreal Engine 5.6 and later. The project is a gameplay-agnostic, provider-agnostic, multi-module Dedicated Server plugin. Core owns common server control-plane mechanisms. Unreal Engine's NetDriver, replication, Iris, and RPC systems continue to handle high-frequency gameplay networking.

## Standards

- [C++ style and header hygiene](Cpp.md)
- [Modules and dependency boundaries](Modules.md)
- [Public API, errors, and documentation](Api.md)
- [Runtime, concurrency, performance, and security](Runtime.md)
- [Build scripts, tooling, and review](BuildAndReview.md)

The [copyright notice rule](BuildAndReview.md#copyright-notices) applies to every project-authored file, subject to format-specific exceptions.

**Must** is mandatory. **Should** is the default unless a PR explains a justified exception. **May** is optional. Public API means declarations intended for use outside their owning module, including Provider contracts and Blueprint-exposed entry points.

## Precedence and exceptions

1. Correctness and Unreal Engine 5.6+ requirements apply first.
2. These explicit project rules override conflicting Epic rules. All other Epic rules remain in force.
3. Pinned formatter output is authoritative for whitespace and layout.
4. A PR requesting an exception must identify the rule, reason, scope, and alternatives considered. A maintainer must approve it. An exception does not silently change the standard for other code.

This standard does not prescribe a project-wide choice between plain C++, UObject, and reflected types. Each module chooses the appropriate form and documents its lifetime and thread behavior.
