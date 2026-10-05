<!-- Copyright (c) 2026 Nelaric Contributors -->

English | [简体中文](FixedObjectPool.zh-CN.md)

# Fixed-capacity native object pools

The `GameplayRuntime` module exposes ordinary C++ pools in `Nelaric::ObjectPool`. Include `ObjectPool/FixedUObjectPool.h` for a custom UObject policy, `ObjectPool/PlainUObjectPoolPolicy.h` for reusable transient UObjects, or `ObjectPool/CharacterPool.h` for authority native character pooling. The consuming module declares a dependency on `GameplayRuntime`.

The pool itself has no reflection macros. Capacity is a template argument, and slot generations, the indexed free list, and object references use inline fixed arrays. Successful prewarm creates every object once. Acquire and Release never expand the pool or replace a lost object. Fixed management storage does not guarantee that animation, physics, collision callbacks, or game-specific work allocate no memory.

## Ownership and lifecycle

A world-scoped native owner or subsystem keeps the pool at a stable address. All operations, reads, construction, destruction, and policy callbacks run on the game thread. Call `Shutdown` before world actors end play or engine resources are cleaned up. Copy and move are disabled. Handles must expire before their pool instance is destroyed and cannot survive reconstruction at the same address.

Each instance permits one prewarm attempt. Creation failure rolls back objects already created and prevents another attempt. Shutdown invalidates outstanding leases before destroying actors or releasing ordinary UObject references. Repeated Shutdown succeeds; it never permits reinitialization. A lease is not an RAII object: copying it does not create another lease, and Release does not clear copies of its raw object pointer.

| Reference mode | Contract |
| --- | --- |
| `Collector` | Default for ordinary UObjects. A non-reflected `FGCObject` reports fixed `TObjectPtr` entries to GC. |
| `WorldWeak` | Default for actors. The world owns them; weak references detect invalidation. Acquire or Release observing a lost object disables further acquisition. Other live leases remain returnable. |
| `WorldRaw` | Explicit actor optimization. External destruction, expiring life spans, and world or level teardown during pool use are forbidden. Shutdown must run first. |

Weak references check object lifetime; generations check logical lease reuse. Neither extends the pool's lifetime. `Get` returns null during transitions or for an invalid lease or object; it does not set the lost-object fault flag. A saturated generation at the free-list head prevents further acquisition without wrapping the counter.

`GetByIndexUnchecked(Index)` is a force-inlined direct array read available only to `WorldRaw` pools. It performs no bounds, game-thread, world, readiness, transition, generation, or object-lifetime checks. There is no allocation, search, or weak-reference resolution; address calculation and the pointer load still have a cost. The caller guarantees `Index < CapacityValue`, successful prewarm, a live pool and actor, game-thread access, and no active lifecycle transition. A compile-time assertion rejects calls in other reference modes.

The accessor returns either a free or a leased slot's object and does not acquire it. `Lease.Handle.Index` identifies the physical slot, but an index alone cannot identify a logical lease after reuse. Keep `Get(Handle)` and `Release(Handle)` for operations that require generation validation. Direct access must end before world teardown or pool shutdown starts, and external actor destruction or expiring life spans are forbidden with `WorldRaw`.

## Character interface and helper

Any `ACharacter` hierarchy can implement the non-reflected `Nelaric::ObjectPool::IPoolableCharacter` interface from `ObjectPool/PoolableCharacter.h`. It provides `PrepareForPool`, `ActivateFromPool`, `DeactivateToPool`, and `IsPoolActive`. The character policy checks this contract at compile time and calls ordinary native virtual functions; it does not use `UInterface`, `ProcessEvent`, or a runtime type registry.

Keep one `FCharacterPoolState` member and forward common transitions to `FCharacterPoolHelper` from `ObjectPool/CharacterPoolHelper.h`. The helper changes native movement, mesh animation pause, visibility, collision, and tick state. Each character implementation supplies its own business reset and cancellation around these calls. The helper does not require GAS or a particular project character base.

The policy uses deferred spawning and calls `PrepareForPool` before `FinishSpawning`, so automatic possession and native gameplay are disabled before BeginPlay. Construction and initialization must preserve the inactive state. Each character implements the interface on its existing base class and delegates common transitions to the helper.

`ADemoCharacter` keeps `ANelaricGasCharacter` as its base and implements the same interface. `ADemoPlayerCharacter` continues to inherit from `ADemoCharacter`. Ordinary demo spawns remain active with their existing possession and replication settings. Pool-created instances start inactive and replicate from authority. Demo activation requires a committed GAS binding whose ASC avatar is this character, both before and after native activation. A failed readiness check returns `ActivationFailed` and restores the free slot. Retained attributes, effects, abilities, and control ownership still follow their existing contracts; the pool does not recreate GAS state per lease.

During pooled demo initialization, a transient `UPawnControlComponent` is added only when neither construction nor the local initialization configuration supplies a control policy. If configuration will create one during BeginPlay, the character subscribes to pawn Ready and waits for that component rather than creating a competing fallback. The character captures the existing policy's four control settings, or the native defaults of that fallback, before disabling player control, handback, and bot startup while idle. Successful activation restores those captured values before enabling collision and explicitly starts any already-bound Ready bot, because the idle Ready notification is not repeated. Return disables the settings again, stops AI movement and the pawn/controller brains, and preserves the snapshot for the next lease. Controller associations and GAS ownership remain with gameplay; resolve player possession and control reservations before returning a lease. Authored false values remain false on activation. The world's GameMode must supply the demo GAS PlayerState layout; `ADemoGameMode` does so. The helper marks prepared states separately from ordinary spawns. DemoGame observes its logical pool state during native visibility transitions on authority and clients, so client selection sees restored local control settings too. Visibility edits alone do not change the control state. These control settings stay in DemoGame; the shared helper and pool lifecycle owner have no dependency on pawn control or GAS.

## Character usage

`ObjectPool/FixedObjectPoolWorldSubsystem.h` provides the plugin's abstract world lifecycle owner. A concrete subsystem keeps its typed native pools as members and implements `PrewarmPools` and `ShutdownPools`. The base calls prewarm once in `OnWorldBeginPlay`, before the game mode dispatches actor BeginPlay. It shuts pools down through `FWorldDelegates::OnWorldBeginTearDown` before world actor EndPlay, and uses `Deinitialize` as an idempotent fallback. Each callback must preserve its world and subsystem; ending the world from a pool transition violates this contract.

`DemoGame` supplies `UDemoCharacterPoolSubsystem` from `ObjectPool/DemoCharacterPoolSubsystem.h`. Unreal creates it automatically in Game and PIE worlds in every net mode. Its native member is `TFixedUObjectPool<ADemoCharacter, Capacity, Nelaric::Demo::FCharacterPoolPolicy, EReferenceMode::WorldRaw>`. The demo policy extends the shared character lifecycle with per-lease team injection. The subsystem constructor resolves `/Game/Demo/Demo1_GrandWarfront/Characters/BP_DemoCharacter` as an `ADemoCharacter` subclass and retains a reflected class reference for GC and cooking. Authority prewarm passes this class through the existing `FCreateArgs::Class` input and creates Capacity inactive Blueprint instances in PersistentLevel at the identity transform. Missing class loading returns `CreationFailed` without falling back to native characters. Clients complete startup without creating a local authority pool. The subsystem exclusively controls authority actor destruction and closes the pool before world actor teardown. Capacity, reference mode, and native base type are compile-time choices; the creation class selects the concrete subclass for all slots. Other consuming games provide their own concrete owner without adding game or GAS dependencies to GameplayRuntime. The shared character pool's default mode remains `WorldWeak`.

Authority gameplay calls start after the world has begun play. Before then, on clients, or once teardown starts, acquisition and return report `NotReady`, `Get` returns null, and `NumFree` reports zero. The separate `GetByIndexUnchecked` accessor bypasses these gates and requires a successfully prewarmed authority pool and valid lifetime. `GetPrewarmResult` preserves the startup outcome for diagnostics; client startup succeeds without local actors. `IsReady` checks current authority world and pool storage availability, not every character's GAS readiness. Before GAS commits, acquisition can still report `ActivationFailed`. Prewarm failure is logged, rolls back created actors, and is never retried on the same instance. Callers do not prewarm or shut down the demo pool manually.

Place `ADemoInitialCharacterSpawnPoint` from `Spawning/DemoInitialCharacterSpawnPoint.h`, or a Blueprint subclass, at each initial character location. It derives from `ATargetPoint`, uses the marker's world transform, and waits for the prewarmed characters to finish BeginPlay, pawn initialization, and committed GAS readiness before attempting one acquisition. Startup readiness is latched separately from storage readiness; later streamed points retain the normal per-lease policy checks. Only authority performs acquisition, including standalone, listen-server, and dedicated-server worlds; client markers never create local characters. Each successful point activates one existing `BP_DemoCharacter` instance. Its TeamId is the first injection entry point: set Team Id under Demo / Spawning in Blueprint defaults or on each level instance. Markers default to 0; 255 is neutral. TryAcquire(Transform, TeamId) assigns the character identity through the demo pool policy before collision and AI activation, and the character replicates its ID to clients. Return stops the character before clearing its team; every new lease injects its own ID. Native calls that omit TeamId acquire a neutral character instead of retaining a previous lease identity. The point's `GetSpawnedCharacter` resolves the generation handle, and `GetSpawnedHandle` exposes the native token for gameplay-managed return. Active leases belong to the world pool; ending the marker cancels its pending startup callbacks and readiness subscription, and world shutdown closes all leases.

At world startup, `UDemoCharacterPoolSubsystem` counts loaded initial spawn points and warns when their number exceeds `Capacity`. A point that cannot acquire a slot also logs a warning and remains empty, including points in levels loaded later. Waiting subscribes to the first pending pawn's existing Ready notification and checks readiness at a bounded 0.05-second interval as a fallback for GAS progress outside that Ready group. Acquisition is deferred outside initialization callbacks and attempted only once. Waiting ends after ten seconds of world time with a warning showing the pending character's pawn and GAS state. Actual activation failures log the rejected GAS or native transition before the point reports its pool error. There is no pool growth, replacement spawn, repeated acquisition, or actor tick. Placement must provide a valid character location because activation does not search for an unblocked spawn position.

Demo characters return automatically four seconds after death through an authority one-shot timer. The subsystem records each successful acquisition's generation token; `ReleaseDeadCharacter` resolves that current lease and rejects live, player-controlled, foreign or unleased actors. A possessed demo character first switches to its retained overview through the control coordinator. Early return, revival and teardown cancel the death timer. The TypeScript death presentation freezes the ragdoll after one second and resets on parking; there is no opacity fade. Acquiring a dead returned slot explicitly resets combat state before activation, while living returned slots retain their existing state. Initial spawn-point handles become stale after automatic return and never resolve a later occupant.

`ADemoRuntimeCharacterSpawnPoint` from `Spawning/DemoRuntimeCharacterSpawnPoint.h` is the second placeable `ATargetPoint` subclass. It is currently an empty runtime-spawning extension point and does not acquire characters or consume pool capacity.

This example runs in `DemoGame` after world BeginPlay:

```cpp
#include "ObjectPool/DemoCharacterPoolSubsystem.h"

UDemoCharacterPoolSubsystem* Pool = World->GetSubsystem<UDemoCharacterPoolSubsystem>();
if (!Pool || !Pool->IsReady())
{
    return;
}

auto Lease = Pool->TryAcquire(SpawnTransform, TeamId);
if (!Lease)
{
    // Lease.Result.Error distinguishes full, busy, lost, and activation failure.
    return;
}

const UDemoCharacterPoolSubsystem::FHandle Handle = Lease.Handle;
ADemoCharacter* Character = Lease.Object;
// Use Character immediately; across frames resolve Pool->Get(Handle).
ADemoCharacter* SameCharacter = Pool->GetByIndexUnchecked(Handle.Index);
// Direct access assumes valid lifetime and does not validate this lease.
const auto ReturnResult = Pool->Release(Handle);
// The owning subsystem closes the pool when this world starts teardown.
```

`Prewarm`, `Release`, and `Shutdown` return `FPoolResult`, with an `EPoolError` field and boolean conversion. A failed acquire carries the same result model in `FLease::Result`. Boolean call sites remain usable. Errors distinguish unavailable state, repeated prewarm, reentry, failed creation or activation, full capacity, exhausted generation, lost objects, and invalid handles.

The shared helper leaves prepared characters hidden, non-colliding, non-damageable, and without automatic possession. Activation requires BeginPlay, applies a transform without a sweep, restores standing posture and walking simulation, enables native ticks and visibility, then enables collision last. Return clears movement, jump, input, accumulated forces, and pending launch, and disables actor, movement, and skeletal-mesh ticks.

The supplied character policy accepts standalone, listen-server, and dedicated-server worlds and spawns into PersistentLevel. It rejects client creation and acquisition even when a local client actor reports `ROLE_Authority`. AI, GAS resets, montages, root-motion sources, cloth, ragdoll, custom components, timers, and asynchronous task cancellation still need a type-specific policy. Ordinary UObject policies also need the correct factory, outer lifetime, and native reset contract; inheritance from UObject alone does not make an asset or component reusable.

## Network lifecycle

| Net mode | Pool ownership and access |
| --- | --- |
| `NM_Standalone` | Local authority prewarms and uses the same synchronous lease API. |
| `NM_ListenServer` | Server prewarms and leases; the local host uses authority actors. |
| `NM_DedicatedServer` | Server prewarms and leases without a local player or viewport. |
| `NM_Client` | Receives server actors; the demo service rejects local leases with `NotReady`. |

Handles contain a local pool address and must never be replicated or used across worlds. Client requests continue through the game's existing authority request path. A synchronous local `TryAcquire` cannot return a remote server lease. Client copies are world-owned proxies, not slots in another local pool.

During prewarm, the helper adds a private replicated actor component once. The policy enables actor and movement replication after initialization, and keeps pooled actors always relevant with owner-only relevancy disabled. Inactive proxies remain present across leases; returning an actor does not disable replication or intentionally destroy the client copy. This trades a fixed set of client actors and replication bookkeeping for avoiding channel recreation on each lease. The default keeps actors awake; it does not automatically apply idle dormancy.

The component replicates one monotonically increasing transition value, including an active bit. A return followed by acquisition between replication updates changes that value even if the final active bit is unchanged. Clients clear native movement and prediction data before applying the latest active state, while retaining UE's received placement and movement mode. Intermediate leases can be coalesced by replication; the value identifies the latest state, not an event stream. The adapter does not require a reflected interface, GAS, or a runtime type registry.

Each character using the helper also binds its native state after initial replication, as `ADemoCharacter` does:

```cpp
void AMyCharacter::PostNetInit()
{
    Nelaric::ObjectPool::FCharacterPoolHelper::PrepareForPool(*this, PoolState);
    Super::PostNetInit();
}
```

On authority, `PrepareForPool` remains a deferred-spawn operation. On clients, it binds the state to an already received private adapter and leaves ordinary non-pooled spawns unchanged. Bind before `Super::PostNetInit` dispatches BeginPlay, so startup callbacks observe the pooled inactive state. Initial replication before actor BeginPlay applies inactivity immediately and defers final activation until BeginPlay finishes. The adapter clears its deferred callback on EndPlay. Client state application changes common presentation and movement; type-specific replicated business state remains the character's responsibility.

Authority activation and return wake the actor before changing replicated properties and force a network update afterwards. Do not change network ownership during a lease without the control coordinator; complete a control handback before reuse. Resetting movement prediction is not a substitute for revoking an old connection's ownership. Custom replication graphs, Iris filters, or relevancy overrides must keep pooled proxies relevant if stable client instances are required.

## Extension and reentry

A custom policy provides `FCreateArgs`, `FAcquireArgs`, and static `Create`, `OnAcquire`, `OnReturn`, and `Destroy` functions. `OnAcquire(false)` must leave the object valid for the infallible `OnReturn`. Native callbacks and the engine callbacks they trigger must preserve the pool and the transitioning object, and must not force GC. Destroy is the shutdown-only destruction path.

Same-pool nested mutations fail with `InTransition`; `Get` returns null while a transition runs. Deactivation completes before a slot becomes free. If overlap callbacks need to acquire, release, or shut down, enqueue bounded commands for execution after the current transition and handle failed returns explicitly.

Existing character types retain their base class and implement `IPoolableCharacter`. Define how idle state revokes Ready bindings, cancels initialization work, and restores state on acquisition; use a custom policy when the default creation or transition contract is insufficient. Pool lease generations and Pawn initialization generations protect different contracts. They do not automatically invalidate each other. Concrete health, targets, AI, GAS, and networking semantics remain in the consuming game or optional integration.

## Validation

Compile the actual consuming template specializations and validate full capacity, repeated reuse, stale handles, failed activation, GC retention, unexpected actor destruction, overlap reentry, and world teardown in the engine. Measure slot management separately from the lifecycle policy and engine work. Memory Insights can inspect allocation call stacks with `-trace=default,memory` in a Development build.
