<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](NelaricCore.zh-CN.md)

# NelaricCore

`NelaricCore` is the runtime module in the `NelaricServer` plugin.

The module owns common gameplay contracts for match flow, objectives, scoring, and player lifecycle. It builds on Unreal Engine's gameplay and networking systems without requiring a specific genre, backend, ability system, or content-distribution provider. Optional integration modules may depend on Core; Core must not depend on them.

Its only direct Unreal module dependency is `Core`, declared private in `NelaricCore.Build.cs`. There are no public module dependencies.
