<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](NelaricFoundation.zh-CN.md)

# NelaricFoundation

`NelaricFoundation` is the runtime module in the `NelaricGameplay/Plugins/NelaricGameplayFoundation` plugin. The `NelaricGameplay/NelaricGameplay.uproject` project enables it, and other projects can use it independently.

The module owns common contracts for rule evaluation, gameplay state, player lifecycle, and activity composition. Match flow, objectives, and scoring belong in optional modules; a persistent world can run without a global match or result. The module builds on Unreal Engine's gameplay and networking systems without requiring a specific genre, backend, ability system, or content-distribution provider. Optional modules may depend on Foundation; Foundation must not depend on them.

Its only direct Unreal module dependency is `Core`, declared private in `NelaricFoundation.Build.cs`. There are no public module dependencies.
