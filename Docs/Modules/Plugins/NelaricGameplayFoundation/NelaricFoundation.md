<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](NelaricFoundation.zh-CN.md)

# NelaricFoundation

`NelaricFoundation` is the runtime module in the `NelaricGameplay/Plugins/NelaricGameplayFoundation` plugin. The `NelaricGameplay/NelaricGameplay.uproject` project enables it, and other projects can use it independently.

The module owns common contracts for rule evaluation, gameplay state, player lifecycle, and activity composition. Match flow, objectives, and scoring belong in optional modules; a persistent world can run without a global match or result. The module builds on Unreal Engine's gameplay and networking systems without requiring a specific genre, backend, ability system, or content-distribution provider. Optional modules may depend on Foundation; Foundation must not depend on them.

`UNelaricWorldStartupConfig` is a `UPrimaryDataAsset` declared in `Public/World/NelaricWorldStartupConfig.h`. It selects a map through the `WorldMap` soft reference and contains both world player policy and defaults for local activities. A startup owner selects and retains the asset before loading its map. `MinPlayersToActivate` is the minimum admitted player count needed to activate world gameplay; `0` removes the gate, while `1` waits for the first player. The world may load and admit players before activation, and falling below the threshold later does not automatically deactivate gameplay. `MaxPlayers` caps admitted players, with `0` adding no framework limit. `bInitiallyAcceptingPlayers` sets the initial admission state, while `bAllowJoinAfterActivation` controls admission after gameplay activates. `HasValidPlayerLimits()` rejects negative values and a positive maximum below the minimum.

In the same startup config, `MinParticipantsToStart` and `MaxParticipants` use `0` for no respective activity limit. `StartPolicy` selects an explicit start or an automatic start when the minimum is met; with a zero minimum, the automatic policy is immediately ready. `JoinInProgressPolicy` rejects or admits participants after an activity starts. `HasValidActivityParticipantLimits()` checks the authored range. `HasValidStartupConfig()` also checks the map reference and enum values, but does not load the map or validate server capacity. These values do not determine whether a running activity ends when participants leave, nor do they override world or server capacity. The asset does not apply its policies or replicate runtime state; authority-side gameplay code must do so on the game thread.

The public asset type requires public dependencies on Unreal's `Core`, `CoreUObject`, and `Engine` modules in `NelaricFoundation.Build.cs`.
