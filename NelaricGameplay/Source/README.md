<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](README.zh-CN.md)

# Demo game module

The project's `Source/` directory contains the `DemoGame` Runtime module and
Game, Editor, and Server targets. All three targets build `DemoGame`, which
registers the project's primary game module.

`DemoGame` is the starting point for project-specific demo gameplay. It
currently provides module scaffolding and has one private dependency, Unreal's
`Core` module, for module registration. Add gameplay classes under `Public/`
and their implementations under `Private/`, declaring further dependencies
when those classes need them.

Reusable gameplay contracts and framework actors live in the `GameplayRuntime`
module of the `NelaricGameplay` plugin. The project configuration selects the
plugin's default gameplay classes. See the
[GameplayRuntime module documentation](../../Docs/Modules/Plugins/NelaricGameplay/GameplayRuntime.md)
for its responsibilities and integration contracts.
