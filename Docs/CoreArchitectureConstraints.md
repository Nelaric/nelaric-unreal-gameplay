<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](CoreArchitectureConstraints.zh-CN.md)

# Core Architecture Constraints

This document defines the target architecture of Nelaric Unreal Server.

## Purpose and responsibility boundaries

Nelaric provides reusable gameplay foundations for Unreal Engine 5.6 and later. A game defines its rules and content through C++ and Blueprint extension points. The framework provides common match flow, objectives, scoring, player lifecycle, and composition mechanisms without prescribing a genre.

Nelaric builds on Unreal Engine's Gameplay Framework and native networking. Game-specific abilities, inventory, AI, presentation, and matchmaking remain with the integrating game or optional modules. Gameplay Ability System, Game Features, and backend services are optional integrations rather than Core requirements.

## Gameplay authority and state

The authoritative world evaluates rules, advances match phases, resolves objectives and scores, and manages player lifecycle. GameMode holds server-only decisions; GameState and PlayerState expose the state clients need. Definitions describe a mode and its rules; runtime state records the progress of a particular match. Clients render and submit input without becoming the source of authoritative outcomes.

## Supported topologies

| Topology | Authority | Requirement |
| --- | --- | --- |
| Standalone | Local world | Gameplay runs without a network connection or backend. |
| Listen server | Host server world | The same rules run on authority and replicate state to remote clients. |
| Dedicated server | Headless server world | The same rules run without a local player or presentation dependency. |

These are Unreal Engine network topologies, not Nelaric compile-time modes. Gameplay rules must not require separate local and online implementations. Backend connectivity is an independent, optional concern; concrete integrations define their own startup, identity, and trust contracts.

## Gameplay composition

Core defines stable contracts for match phases, rule evaluation, objectives, scoring, and player lifecycle. A mode combines these mechanisms and supplies game-specific policy. Public extension points support C++ and Blueprint where appropriate. Core must not require a particular character class, camera, input scheme, ability system, or asset layout.

## Content updates

Versioned cooked gameplay data and content may be delivered separately from the base build. A content version identifies its compatible build, required assets, and activation boundary. A running match keeps one version for its lifetime; new content activates at a defined safe boundary such as the next match or process start. Participants in one network match must use compatible content and protocol versions.

Native C++ code, reflected type layouts, and network protocol changes require a compatible new build and rollout. Editor Live Coding is a development facility, not the runtime content-update contract. Distribution channels and platform-specific patch delivery remain outside Core.

## Validation

The framework must support authoring and testing the same mode in standalone, listen-server, and dedicated-server sessions. A first acceptance case is an objective-and-score mode in all three topologies. A second distinct mode must be built through the public extension points without changing Core. Content-update tests must cover version compatibility and activation at the documented boundary.

This document applies together with the [module boundaries](CodingStandards/Modules.md) and [runtime rules](CodingStandards/Runtime.md).