<!-- Copyright (c) 2026 Nelaric -->

# Gameplay template modules

The project builds four independent Runtime modules. Each module owns
Blueprintable GameMode, PlayerController, and PlayerState classes and can
be selected per map. Each GameMode selects its own controller and player
state by default. None of the four modules includes or depends on another
template.

All four depend on Unreal's Core, CoreUObject, and Engine modules and the
shared NelaricFoundation module. They derive from its game mode and player
controller bases to retain the common session-transition setup.

| Module | Game mode | Player controller | Player state |
| --- | --- | --- | --- |
| `NelaricOpenWorldTemplate` | `ANelaricOpenWorldGameMode` | `ANelaricOpenWorldPlayerController` | `ANelaricOpenWorldPlayerState` |
| `NelaricBattleRoyaleTemplate` | `ANelaricBattleRoyaleGameMode` | `ANelaricBattleRoyalePlayerController` | `ANelaricBattleRoyalePlayerState` |
| `NelaricMobaTemplate` | `ANelaricMobaGameMode` | `ANelaricMobaPlayerController` | `ANelaricMobaPlayerState` |
| `NelaricSandboxTemplate` | `ANelaricSandboxGameMode` | `ANelaricSandboxPlayerController` | `ANelaricSandboxPlayerState` |

These classes are starting points for project-specific rules and player data.
Create a derived Blueprint or C++ class and assign it to the relevant map.
Keep authoritative decisions on the server and replicate client-visible
state through Unreal's GameState, PlayerState, and actor replication.
