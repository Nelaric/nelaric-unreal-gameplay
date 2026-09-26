<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](Cpp.zh-CN.md)

# C++ Style and Headers

Follow both the [Epic C++ standard](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) and the project rules below. Report any conflict in an issue instead of choosing one rule over the other. The project uses clang-format for layout; run the pinned version before submitting a PR.

Use tabs for block indentation in C++ and UE C# build scripts, with a tab width of four characters. Spaces may align text after non-tab characters. This follows Epic's indentation rule; the formatter configuration enforces it.

## Names and types

- Follow all applicable Unreal type prefixes and naming rules. Put project-owned non-reflected C++ types and APIs in `Nelaric::`, with a nested domain namespace where useful; for example, `Nelaric::FMatchId` and `Nelaric::IMatchRule`. Do not repeat `Nelaric` in a type name solely because it is inside this namespace. Unreal Header Tool does not support namespaces for reflected types, so globally scoped project-owned reflected types include `Nelaric` after the UE prefix, for example `UNelaricMatchSubsystem`. Unreal-required symbols such as module entry points are exempt.
- Name modules and log categories consistently around `Nelaric` and the owning domain. Prefer names that describe mechanism or responsibility rather than a particular game's rules.
- Follow Epic's standard-library guidance. Prefer UE containers and strings; avoid standard-library containers and strings except in interoperability code. Other standard-library facilities may be used where Epic permits them and they give better results. Stable cross-module public APIs use UE types; do not mix UE and standard-library conventions in one API.
- Prefer typed constants and `constexpr` to new macros. Use required UE macros normally. A new project macro needs a documented reason and follows Epic's uppercase `UE_` naming rule; cross-module feature switches must be defined centrally.

## Headers and dependencies

- Public headers must be self-contained: a consumer can include one without relying on incidental include order. Include what the declaration needs and forward-declare where that is sufficient.
- Put implementation-only includes and declarations in `Private`. A Foundation public header must not include a concrete optional integration or vendor SDK header.
- Keep header dependencies small. Do not use a broad include solely to obtain a forward-declarable type.
- Follow Unreal Header Tool requirements for reflected declarations, including the placement of generated headers. These requirements take precedence over mechanical include sorting.

## UCLASS exports

- Every project-authored `UCLASS` must explicitly include `MinimalAPI` in its `UCLASS(...)` declaration. This applies to all modules and to classes in both `Public` and `Private`, including abstract, internal, editor, and test classes.
- Do not put the owning module's `*_API` macro on a `UCLASS` class declaration. Whole-class export is forbidden; retain `MinimalAPI` when adding cross-module functionality.
- Export only individual non-inline methods that require cross-module C++ linkage, by placing the owning module's `*_API` macro on those method declarations. Include constructors and other methods needed for supported cross-module derivation where necessary. Do not export implementation-only methods merely because they are public or reflected.
- `MinimalAPI` controls native symbol exports; it does not replace `UFUNCTION` or Blueprint exposure specifiers. Type visibility alone does not export non-inline method implementations. See Epic's [class specifiers](https://dev.epicgames.com/documentation/unreal-engine/class-specifiers) for the engine semantics.

## Source text

Code and human-written documentation use UTF-8 with BOM and CRLF. Tool configuration and executable scripts may use UTF-8 without BOM and LF where required for interoperability; the exceptions are listed in [Build scripts and tooling](BuildAndReview.md). Do not mix newline styles within a file.
