<!-- Copyright (c) 2026 Nelaric -->

# Gameplay template modules

The project builds four independent Runtime modules for the acceptance
examples in [Foundation architecture constraints](../../Docs/FoundationArchitectureConstraints.md).
Each module owns Blueprintable GameMode, PlayerController, and PlayerState
classes and can be selected per map. Each GameMode selects its own controller
and player state by default. None of the four modules includes or depends on
another template.
All four depend only on Unreal's Core, CoreUObject, and Engine modules and the
shared NelaricFoundation plugin. They derive from its game mode and player
controller bases to retain the common session-transition setup.

| Module | Game mode | Intended extension |
| --- | --- | --- |
| `NelaricOpenWorldTemplate` | `ANelaricOpenWorldGameMode` | Keep the world running while regional activities start and finish independently; connect player and quest progress to an optional persistence adapter. |
| `NelaricBattleRoyaleTemplate` | `ANelaricBattleRoyaleGameMode` | Add an activity with participants, phases, elimination, and a result. The activity ends without ending its World. |
| `NelaricMobaTemplate` | `ANelaricMobaGameMode` | Add an activity with teams, concurrent objectives, optional score or resources, and victory rules. |
| `NelaricSandboxTemplate` | `ANelaricSandboxGameMode` | Run player state, world events, and optional rules without a global objective, score, or ending. |

These classes are starting points for project-specific rules and player data.
The player state classes contain no custom replicated fields yet. They do not
implement combat, inventory, quests, persistence, objectives, or scoring.
Create a derived Blueprint or C++ class and assign it to the relevant map.
Keep authoritative decisions on the server and replicate client-visible
state through Unreal's GameState, PlayerState, and actor replication. The
same rules should run in standalone, listen-server, and dedicated-server
worlds.
