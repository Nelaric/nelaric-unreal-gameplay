<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](FoundationArchitectureConstraints.zh-CN.md)

# Foundation Architecture Constraints

This document describes the implemented responsibilities and integration boundaries of Nelaric Unreal Gameplay.

## Purpose and responsibility boundaries

Nelaric provides reusable gameplay foundations for Unreal Engine 5.6 and later. `NelaricFoundation` contains Pawn and Character bases, pawn context queries, component initialization contracts, world startup configuration, and client-to-client session transitions. Four independent Runtime template modules provide Blueprintable GameMode, PlayerController, and PlayerState classes for game-specific extension.

Foundation builds on Unreal Engine's Gameplay Framework and native networking. It publicly depends on `Core`, `CoreUObject`, `Engine`, and `OnlineSubsystemUtils`. The project enables the Foundation and PuerTS plugins. Concrete gameplay rules and content belong to the consuming game.

## Gameplay authority and state

`ANelaricGameModeBase` is the project's configured default GameMode and selects `ANelaricPlayerController`. On listen servers and dedicated servers, it starts the transition approval beacon. The player controller carries departure approval between the current server and its owning client. Target approval is handled through an Online Beacon and the target GameMode.

Pawn initialization state, generation, and managed dynamic component instances are local to each peer. Gameplay data uses separate Unreal replication paths. A data-arrival event can request a local initialization refresh. Pawn components query the current local pawn context; they do not cache controller or player state pointers.

## Gameplay lifecycles

`ANelaricPawn` and `ANelaricCharacter` each own an initialization component. A `UNelaricPawnInitializationConfig` asset selects participant classes, creation sides, required components, and local Ready dependencies. Participants own the ordered states Registered, DataAvailable, DataInitialized, and Ready. The world subsystem coordinates progress; cyclic dependencies commit as a group before notification.

Pawn readiness combines the required participant states with a pawn-specific context check. Replacing the configuration revokes readiness and replaces managed instances. Participant generation invalidation rejects stale asynchronous work; EndPlay and world teardown stop initialization and clean up managed instances. See [Pawn component initialization](API/Pawn/PawnInitialization.md).

## Runtime topologies

| Context | Implemented behavior |
| --- | --- |
| Standalone | Configured pawn components use the authority creation flag. |
| Listen server | The server pawn uses authority entries; remote clients create their local client entries. The GameMode starts the approval beacon. |
| Dedicated server | The server pawn uses authority entries. The GameMode starts the approval beacon without a local player. |
| Client | Configured pawn components use the client creation flag. The session subsystem can request a move to another remote server. |

These contexts use Unreal's network roles. The initialization manager chooses entries from the owning pawn's `HasAuthority()` result. Session transitions accept a client source and client target, then verify a new client world and its destination connection. See [network sessions and authority transitions](API/Core/NetWork/NetworkSessionTransitions.md).

## Gameplay composition

`UNelaricPawnComponent` provides pawn context queries; `UNelaricGameplayComponent` makes that base available as a Blueprint-spawnable component. Initialization participants implement `INelaricInitStateParticipantInterface` directly or derive from `UNelaricPawnInitStateComponent`. Their declared dependencies control entry into Ready, while each component supplies its own data preparation and gameplay behavior.

`UNelaricWorldStartupConfig` stores a map soft reference, player-count policy, and activity participant defaults. Its validation functions check the authored count ranges, map reference, and policy enum values. The asset is configuration data rather than replicated runtime state.

## Content and development tooling

The project includes PuerTS with selectable V8, QuickJS, and Node.js backends. Setup scripts prepare TypeScript editor tooling. The editor integration supports TypeScript compilation and script hot reload. Shared framework assets belong in `NelaricGameplay/Content`; game-specific maps, characters, and rules belong to consuming games or optional features.

## Validation

The Foundation module includes automated tests for pawn context queries, initialization lifecycle gates, dependency graphs, generation invalidation, local server/client initialization, and client-to-client approval coordination. Framework tests use the `Nelaric.*` hierarchy. The Editor CI job runs compatible tests after compilation; Game, Editor, and Server targets build separately with UE 5.6.1 on Linux.

This document applies together with the [module boundaries](CodingStandards/Modules.md) and [runtime rules](CodingStandards/Runtime.md).
