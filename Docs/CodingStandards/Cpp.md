# C++ Style and Headers

Follow the [Epic C++ standard](https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) unless an explicit rule below differs. The project uses clang-format for layout; run the pinned version before submitting a PR.

Use tabs for block indentation in C++ and UE C# build scripts, with a tab width of four characters. Spaces may align text after non-tab characters. This follows Epic's indentation rule; the formatter configuration enforces it.

## Names and types

- Use Unreal's type prefixes (`U`, `A`, `F`, `I`, `E`, and `T`) as applicable. Project-owned public types must include `Nelaric` after the UE prefix, for example `FNelaricSessionId` or `UNelaricServerSubsystem`. This avoids collisions when the plugin is installed alongside other plugins. Unreal-required symbols such as module entry points are exempt.
- Name modules and log categories consistently around `Nelaric` and the owning domain. Prefer names that describe mechanism or responsibility rather than a particular game's rules.
- Prefer UE containers, strings, delegates, smart pointers, and facilities for UE-facing code. Standard-library facilities may be used internally where appropriate, but a stable cross-module public API must use UE types. Do not expose two parallel conventions for the same API.
- Prefer typed constants and `constexpr` to new macros. Use required UE macros normally. A new project macro needs a documented reason; cross-module feature switches must be defined centrally.

## Headers and dependencies

- Public headers must be self-contained: a consumer can include one without relying on incidental include order. Include what the declaration needs and forward-declare where that is sufficient.
- Put implementation-only includes and declarations in `Private`. A Core public header must not include a concrete Provider or vendor SDK header.
- Keep header dependencies small. Do not use a broad include solely to obtain a forward-declarable type.
- Follow Unreal Header Tool requirements for reflected declarations, including the placement of generated headers. These requirements take precedence over mechanical include sorting.

## Source text

Code and human-written documentation use UTF-8 with BOM and CRLF. Tool configuration and executable scripts may use UTF-8 without BOM and LF where required for interoperability; the exceptions are listed in [Build scripts and tooling](BuildAndReview.md). Do not mix newline styles within a file.
