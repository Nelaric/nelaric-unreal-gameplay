<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](Modules.zh-CN.md)

# Modules and Dependency Boundaries

The plugin is divided into modules with `Public` and `Private` directories. Public headers are contracts; private implementation details must not leak into them.

- The GameplayRuntime module must not depend on a vendor SDK, online backend, content-distribution provider, or optional gameplay integration. Optional modules may depend on GameplayRuntime. Circular module dependencies are forbidden.
- Public dependencies in `.Build.cs` must reflect types genuinely required by public headers. Put implementation dependencies in private dependencies. Do not make a dependency public merely to fix a missing include in another module.
- Define gameplay extension contracts in `Nelaric::` when they are non-reflected C++ interfaces. Expose Blueprint entry points where gameplay authors need them; do not force reflection onto every implementation.
- Keep GameplayRuntime genre-independent. GameplayRuntime owns common rule, state, player-lifecycle, and activity-composition contracts. Match flow, objectives, and scoring belong in optional modules and are not prerequisites for every mode. Concrete combat, inventory, quests, and character rules belong to the consuming game or optional extensions.
- Use UE networking for gameplay state replication. GameplayRuntime must not implement a second transport or require a backend for standalone play.
- Keep content-version and update-delivery mechanisms separate from match rules. GameplayRuntime may consume a validated content version without depending on a particular patching service.

Every module should state its responsibility and direct dependencies in its module documentation when it is introduced. A new dependency crossing these boundaries requires a PR explanation and maintainer review.

## Plugin and module structure

The framework uses one project-owned plugin, `NelaricGameplay`, under `NelaricGameplay/Plugins/NelaricGameplay/`. Its four modules correspond to the Runtime, Diagnostics, Benchmark, and Editor responsibilities. Add framework capabilities as modules in this plugin, with responsibilities and direct dependencies documented explicitly. Third-party plugins such as PuerTS remain separate and retain their own names and attribution.

| Responsibility | Module directory under the plugin's `Source/` | Module type |
| --- | --- | --- |
| Runtime | `GameplayRuntime/` | `Runtime` |
| Diagnostics (performance analysis) | `Diagnostics/` | `Runtime` |
| Benchmark | `Benchmark/` | `DeveloperTool` |
| Editor | `Editor/` | `Editor` |

`GameplayRuntime` provides the shared gameplay runtime, including reusable framework actors, initialization and lifecycle coordination, world startup configuration, and session transitions. `Diagnostics` owns performance analysis support; `Benchmark` owns reproducible performance scenarios and comparisons; `Editor` owns editor-only authoring tools. Module naming follows the optional project-prefix rule in [C++ names and types](Cpp.md#names-and-types).

`Diagnostics` registers `ng.Perf.HUD` on startup and unregisters it on shutdown. Its default value is `0` (hidden); a non-zero value shows an English Slate HUD with FPS, Frame, GT, RT, and GPU timings. The HUD covers game viewports and the active level editor viewport, remains enabled after PIE ends, and never consumes input. Measurements are engine-wide; GPU timings use the primary rendering GPU, its adapter name comes from the RHI, and unavailable values display `N/A`. `UFrameworkPerformanceSubsystem` provides an initial game-instance subsystem type with no performance operations; HUD ownership remains in the module so the editor overlay survives PIE teardown. Its public base type requires public `Core`, `CoreUObject`, and `Engine` dependencies in every target. Client builds additionally depend privately on `Slate`, `SlateCore`, `RenderCore`, and `RHI`; editor builds additionally depend privately on `LevelEditor` and `UnrealEd`. Server builds do not create a HUD. `Benchmark` and `Editor` still contain lifecycle scaffolding with a private `Core` dependency; benchmark execution and editor authoring tools are not implemented yet. Add dependencies when an implementation needs another module; GameplayRuntime must remain independent of these optional tools. `DeveloperTool` modules are available only when the target builds developer tools; `Editor` modules are available only to editor targets.

## Internal integration convention

Framework-only C++ methods exposed in GameplayRuntime public headers for calls within that module may accept `const Nelaric::FGameplayRuntimeInternalAccessKey&`. The key and `FGameplayRuntimeInternalAccess::Key()` live in `GameplayRuntime/Private/Internal/` and are used only by that module's implementation. Keep these methods in the framework-integration `public:` section, name them for their internal purpose, and use ordinary C++ rather than `UFUNCTION` for the non-reflected key parameter. Do not introduce inheritance solely to obtain the key.

The key marks implementation-only calls within GameplayRuntime; it is not an authorization boundary. Public headers may forward-declare `Nelaric::FGameplayRuntimeInternalAccessKey` for C++ integration signatures, but must not include the private definition or expose a way for gameplay modules to obtain it. Keep callers in the owning module's `Private` directory. Framework consumers should use supported gameplay APIs. If a future cross-module integration needs access, design and document an explicit public contract and its module dependency.
