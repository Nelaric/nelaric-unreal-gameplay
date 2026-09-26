<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](Modules.zh-CN.md)

# Modules and Dependency Boundaries

The plugin is divided into modules with `Public` and `Private` directories. Public headers are contracts; private implementation details must not leak into them.

- The Foundation module must not depend on a vendor SDK, online backend, content-distribution provider, or optional gameplay integration. Optional modules may depend on Foundation. Circular module dependencies are forbidden.
- Public dependencies in `.Build.cs` must reflect types genuinely required by public headers. Put implementation dependencies in private dependencies. Do not make a dependency public merely to fix a missing include in another module.
- Define gameplay extension contracts in `Nelaric::` when they are non-reflected C++ interfaces. Expose Blueprint entry points where gameplay authors need them; do not force reflection onto every implementation.
- Keep Foundation genre-independent. Foundation owns common rule, state, player-lifecycle, and activity-composition contracts. Match flow, objectives, and scoring belong in optional modules and are not prerequisites for every mode. Concrete combat, inventory, quests, and character rules belong to the consuming game or optional extensions.
- Use UE networking for gameplay state replication. Foundation must not implement a second transport or require a backend for standalone play.
- Keep content-version and update-delivery mechanisms separate from match rules. Foundation may consume a validated content version without depending on a particular patching service.

Every module should state its responsibility and direct dependencies in its module documentation when it is introduced. A new dependency crossing these boundaries requires a PR explanation and maintainer review.

## Internal integration convention

Framework-only C++ methods exposed in Foundation public headers for calls within that module may accept `const Nelaric::FFoundationInternalAccessKey&`. The key and `FFoundationInternalAccess::Key()` live in `NelaricFoundation/Private/Internal/` and are used only by that module's implementation. Keep these methods in the framework-integration `public:` section, name them for their internal purpose, and use ordinary C++ rather than `UFUNCTION` for the non-reflected key parameter. Do not introduce inheritance solely to obtain the key.

The key marks implementation-only calls within Foundation; it is not an authorization boundary. Public headers may forward-declare `Nelaric::FFoundationInternalAccessKey` for C++ integration signatures, but must not include the private definition or expose a way for gameplay modules to obtain it. Keep callers in the owning module's `Private` directory. Framework consumers should use supported gameplay APIs. If a future cross-module integration needs access, design and document an explicit public contract and its module dependency.
