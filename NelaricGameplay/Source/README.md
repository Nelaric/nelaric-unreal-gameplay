<!-- Copyright (c) 2026 Nelaric Contributors -->

English | [简体中文](README.zh-CN.md)

# Demo game module

The project's `Source/` directory contains the `DemoGame` Runtime module and
Game, Editor, and Server targets. All three targets build `DemoGame`, which
registers the project's primary game module.

`DemoGame` is the starting point for project-specific demo gameplay.
`ADemoCharacter` derives from the framework's `ANelaricCharacter`, and
`ADemoPlayerCharacter` derives from `ADemoCharacter` for player-controlled
variants. `UDemoPlayerInputComponent` extends the framework's native input
participant; add it to a player character's pawn initialization configuration.
Assign a `UNelaricInputConfig` with `InputTag.Move` (Axis2D),
`InputTag.Look.Mouse` (Axis2D), `InputTag.Look.Stick` (Axis2D), and
`InputTag.Jump` (Boolean) actions and their mapping contexts. The component
binds native movement, camera, and jump callbacks without GAS. These classes
are available to C++ and Blueprint. The module publicly depends on `Core`,
`CoreUObject`, `Engine`, `GameplayRuntime`, `EnhancedInput`, and `GameplayTags`
because its public character and input headers expose framework and engine
types. Add further gameplay classes under
`Public/` and their implementations under `Private/` as needed.

Reusable gameplay contracts and framework actors live in the `GameplayRuntime`
module of the `NelaricGameplay` plugin. The project configuration selects the
plugin's default gameplay classes. See the
[GameplayRuntime module documentation](../../Docs/Modules/Plugins/NelaricGameplay/GameplayRuntime.md)
for its responsibilities and integration contracts.
