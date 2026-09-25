<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](NelaricGameplayCore.zh-CN.md)

# NelaricGameplayCore

`NelaricGameplayCore` is the runtime module of the `NelaricGameplay/NelaricGameplay.uproject` project. The Game, Editor, and Server targets all include it. The module owns the project's common gameplay World Settings base; reusable gameplay contracts remain in the separate [NelaricFoundation](../Plugins/NelaricGameplayFoundation/NelaricFoundation.md) plugin module.

Gameplay World Settings classes used by this project must derive from `ANelaricWorldSettings`. The project config selects this class as the default World Settings class. A gameplay map that needs additional settings should use a subclass so the shared base remains available. This requirement applies to the `NelaricGameplay` project; projects that use the Foundation plugin independently choose their own World Settings class.

`ANelaricWorldSettings` is an empty base type. A world startup configuration selects its map through a soft reference before the map loads; maps do not select their startup policy through World Settings. Projects may subclass this base for map-specific settings.

`ANelaricWorldSettings` is declared in the public `NelaricWorldSettings.h` header. The module has public dependencies on Unreal's `Core`, `CoreUObject`, and `Engine` modules because its public World Settings type derives from `AWorldSettings`. The startup asset remains in the separate Foundation plugin, without a Core dependency until runtime startup integration is added.
