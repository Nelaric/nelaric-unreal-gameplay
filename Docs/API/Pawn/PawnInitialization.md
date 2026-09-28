<!-- Copyright (c) 2026 Nelaric -->

English | [简体中文](PawnInitialization.zh-CN.md)

# Pawn Component Initialization

## Pawn bases and context

`ANelaricPawn` retains Unreal's `APawn` defaults. `ANelaricCharacter` retains `ACharacter`'s capsule, mesh, and movement component. Both are Blueprintable and create a `PawnInitializationComponent` default subobject.

`UNelaricPawnComponent` queries its direct owning pawn, current controller, player controller, and player state. Queries return non-owning pointers and read current local state without caching. Player state is read directly from the pawn, so it can be available without a local controller. A missing, invalid, or destroying object produces a null result. Tick and component replication are disabled by default. `UNelaricGameplayComponent` exposes this base as a Blueprint-spawnable component.

## Authored configuration

`UNelaricPawnInitializationConfig` is a `UDataAsset` containing `FNelaricPawnInitializationEntry` values. Assign it to the initialization component's `InitializationConfig` property on a derived Pawn or Character Blueprint.

| Field | Meaning |
| --- | --- |
| `ComponentId` | Stable, unique ID used for instance names and dependency references. |
| `ComponentClass` | Concrete actor component class implementing `INelaricInitStateParticipantInterface`. |
| `bCreateOnAuthority` | Create on authority, including standalone play; defaults to `true`. |
| `bCreateOnClient` | Create on non-authority peers; defaults to `true`. |
| `bRequiredForPawnReady` | Include the component in local pawn readiness; defaults to `true`. |
| `DependencyIds` | IDs of local participants required for this component to enter Ready. |

Before creating components, the manager checks IDs, classes, dependency declarations, creation-side compatibility, and collisions with existing instance names or configured IDs. Invalid configuration blocks pawn readiness. Selected instances use names of the form `NelaricInit_<ComponentId>`. The manager creates the complete local component set, resolves its dependency graph, and configures the world subsystem before registering those instances.

Authority and clients create their own selected instances. Managed dynamic instances have replication disabled; initialization state and generation are local. Replicate gameplay data through separate Unreal paths, then request a local refresh when required data arrives.

## Component states and dependencies

Each participant owns its state, generation, and terminal failure flag. `UNelaricInitStateWorldSubsystem` keeps weak registrations and coordinates progress on the game thread.

| State | Meaning |
| --- | --- |
| `Registered` | The participant is registered and can acquire its data. |
| `DataAvailable` | Required local data is available for initialization. |
| `DataInitialized` | Local data has been initialized; final preparation and dependency checks precede Ready. |
| `Ready` | Component gameplay may run. Work requiring the complete pawn also waits for pawn readiness. |

`UNelaricPawnInitStateComponent` is an optional C++ base implementing the participant interface. Override `CanEntryDataAvailable()`, `CanEntryDataInitialized()`, and `CanEntryReady()` for retry-safe local preparation. These gates default to `true`. Start component gameplay in `OnInitReady()`. A pending gate is retried after `RequestInitRefresh()`, for example when an `OnRep` updates required gameplay data.

Dependencies constrain entry into Ready. Dependencies outside a cyclic group must already be Ready. Cyclic participants form a readiness group: every member must reach DataInitialized and pass local preparation, then the coordinator commits all members to Ready before notifying them. A successful `CanEntryReady()` is retained for the current generation.

## Pawn readiness and cleanup

The initialization component attempts readiness at BeginPlay. Pawn readiness requires a valid owning pawn, a valid configuration, all locally required participants at Ready, and `CanInitializePawn()`. That BlueprintNativeEvent accepts a valid owning pawn by default. Context changes can be followed by `TryInitializePawn()`; `IsPawnInitialized()` queries the current readiness conditions. With a null configuration, the manager creates no components and readiness uses the pawn context gate.

`OnPawnInitialized` broadcasts on each entry into Ready; bind before BeginPlay to observe immediate initialization. `OnPawnInitializationRevoked` broadcasts when readiness is revoked. Required participant invalidation, configuration replacement, and teardown can revoke pawn readiness. `SetInitializationConfig()` replaces a different configuration during play by revoking readiness, destroying managed instances, creating the new set, and attempting readiness again. EndPlay and world teardown stop initialization and destroy managed instances.

`InvalidateInitGeneration()` increments the generation before invalidation hooks and work cancellation, then resets the component to Registered and clears terminal failure. When a Ready dependency is invalidated or fails, the coordinator invalidates its configured dependents. `MarkTerminalInitFailure()` records failure separately from the ordered states and stops progress for that attempt.

For asynchronous results, retain a weak component reference and the captured world and generation. After returning to the game thread, use `ResolveInitResult()` or `CanApplyInitResult()` before applying the result. These checks reject invalid, unregistered, departing, failed, or different-generation components and a different world. Derived components release listeners and outstanding work through `OnInitGenerationInvalidated()` and `CancelInitGenerationWork()`.
