<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](FoundationArchitectureConstraints.zh-CN.md)

# Foundation Architecture Constraints

This document defines the target architecture of Nelaric Unreal Gameplay.

## Purpose and responsibility boundaries

Nelaric provides reusable gameplay foundations for Unreal Engine 5.6 and later. A game defines its rules and content through C++ and Blueprint extension points. Foundation provides common contracts for gameplay state, rule evaluation, player lifecycle, and composition. Match flow, objectives, and scoring are capabilities a game may compose rather than a required path for every game.

Nelaric builds on Unreal Engine's Gameplay Framework and native networking. Game-specific abilities, inventory, AI, presentation, and matchmaking remain with the integrating game or optional modules. Gameplay Ability System, Game Features, and backend services are optional integrations rather than Foundation requirements.

## Gameplay authority and state

The authoritative world evaluates rules and manages player lifecycle. It also advances phases and resolves objectives or scores for activities that use those capabilities. GameMode holds server-only decisions; GameState and PlayerState expose the state clients need. Definitions describe rules and enabled capabilities; runtime state records the progress of a world or activity. Clients render and submit input without becoming the source of authoritative outcomes.

## Gameplay lifecycles

- **Persistent world:** The world does not depend on a global match start or end. Regional events, quest progress, and player state can evolve independently. The integrating game or an optional adapter owns persistence and defines the restore boundary. The world itself needs no global objective, score, or win condition.
- **Bounded activity:** Gameplay that needs a start and resolution is represented as a distinct activity. It has its own participants and runtime state, with phases, objectives, scoring, or win conditions enabled as needed. Its start, end, and result apply to that activity without implying that the whole world ends.
- **Nested local activity:** A persistent world or another activity can host multiple local activities, concurrently or in a parent-child relationship. Each activity has a distinct identity, state, and scope. Game rules define what happens to other activities when a parent ends or a child changes; Foundation does not force cascading termination. Rules may write a local result back to parent or world state.

## Supported topologies

| Topology | Authority | Requirement |
| --- | --- | --- |
| Standalone | Local world | Gameplay runs without a network connection or backend. |
| Listen server | Host server world | The same rules run on authority and replicate state to remote clients. |
| Dedicated server | Headless server world | The same rules run without a local player or presentation dependency. |

These are Unreal Engine network topologies, not Nelaric compile-time modes. Gameplay rules must not require separate local and online implementations. Backend connectivity is an independent, optional concern; concrete integrations define their own startup, identity, and trust contracts.

## Gameplay composition

Foundation defines stable contracts for rule evaluation, state ownership, player lifecycle, and activity composition, while optional modules provide phase, objective, and scoring mechanisms when needed. A game selects the mechanisms it needs and supplies game-specific policy. Unused mechanisms must not be prerequisites for startup, operation, or shutdown. Public extension points support C++ and Blueprint where appropriate. Foundation must not require a particular character class, camera, input scheme, ability system, or asset layout.

## Content updates

Versioned cooked gameplay data and content may be delivered separately from the base build. A content version identifies its compatible build, required assets, and activation boundary. A running activity keeps one version for its lifetime. A persistent world may activate new content at process start, a world maintenance window, or another explicit safe boundary, without waiting for a nonexistent global resolution. Compatibility between local activity activation and the parent version must be defined. Participants in one network activity must use compatible content and protocol versions.

Native C++ code, reflected type layouts, and network protocol changes require a compatible new build and rollout. Editor Live Coding is a development facility, not the runtime content-update contract. Distribution channels and platform-specific patch delivery remain outside Foundation.

## Validation

The framework must support authoring and testing the same rules in standalone, listen-server, and dedicated-server sessions. The first set of acceptance cases covers distinct lifecycles; each case must use public extension points without changing Foundation:

| Case | Required capabilities to validate |
| --- | --- |
| Open-world exploration | The world keeps running; regional events or local activities start and end independently; a persistence adapter can save and restore player state and quest progress. |
| Battle royale | An activity starts, changes phases, and resolves; it manages participants, elimination, and a win condition. |
| MOBA | Multiple objectives progress concurrently within one activity; it manages teams, objective state, scoring or resources as needed, and a win condition. |
| Building sandbox | The world may have no global objective or end; world events, player state, and optional rules work without scoring or a win condition. |

Content-update tests must cover version compatibility and activation at the specified boundaries for both persistent worlds and bounded activities.

This document applies together with the [module boundaries](CodingStandards/Modules.md) and [runtime rules](CodingStandards/Runtime.md).
