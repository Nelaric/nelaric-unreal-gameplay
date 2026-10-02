<!-- Copyright (c) 2026 Nelaric Contributors -->

English | [简体中文](ControlState.zh-CN.md)

# GAS state across control changes

`GameplayAbilitiesIntegration` is an optional runtime module in NelaricGameplay. It depends publicly on Core, CoreUObject, Engine, GameplayRuntime, GameplayAbilities, GameplayTags and GameplayTasks; NetCore is private for active-effect replication. GameplayRuntime does not depend on this module. Concrete combat attributes and abilities live in the consuming game.

## Participants and storage

Use `ANelaricGasGameMode`, `ANelaricGasPlayerState` and `ANelaricGasPlayerController`, or derive game classes from them. Humans and `ANelaricBotController` receive the same game-mode PlayerState class. Configure `AttributeSetClasses` for every attribute used by pawn profiles. Participant defaults initialize once through `ParticipantProfile`. Avatar changes keep the PlayerState and its ASC object identity.

`ANelaricGasCharacter` provides PawnGasBindingComponent and IAbilitySystemInterface. Add exactly one PawnControlComponent through a pawn initialization configuration or Blueprint to enable control switching. Other pawn classes can add those components and forward their ability-system interface to the binding. Set `StateProfile` before initialization. It is immutable during the pawn lifetime; conflicting participant/pawn ownership for the same attribute is rejected.

An unpossessed pawn retains its state in an authority-created custody PlayerState ASC. This actor is owned by the pawn, participates in reservations, and is destroyed with the pawn. `IsStateCustodian()` distinguishes it from a gameplay participant for roster logic. Native Pawn.PlayerState can be null while the binding refers to custody; this is intentional. Custody receives no control bonuses and cannot submit input without a controller.

## Logical ownership

| State | Owner | Control-change behavior |
| --- | --- | --- |
| Profile attribute with Participant ownership, or an omitted attribute | Participant | Remains on that participant ASC. |
| Profile attribute with Pawn ownership | Pawn | Snapshot its base, clear the old role slot and import on the destination ASC. |
| Pawn profile ability grant | Pawn | Preserve class, level, action tags, input ID and SetByCaller values; reconstruct on destination. |
| Untracked active effect | Participant | Remains on its ASC. |
| Active effect marked Pawn using `TrackEffect` | Pawn | Restore its spec, stack count, inhibition, remaining duration and periodic deadline. |
| Active effect marked Control | Relationship | Remove on release; configure fresh bonuses in the destination profile. |

All attribute storage remains in AttributeSets on the PlayerState ASC. There is no second persistent health store on the pawn. Choose health ownership per game. Use `ControlEffects`, `PlayerControlEffects` and `BotControlEffects` for reversible operating bonuses; control effects must be non-instant and use None stacking, so their handles cannot merge into pawn or participant state. Current values are recalculated from imported bases and active modifiers. Raw Control attribute rules are rejected because a reversible bonus needs an effect lifetime.

Mark role effects immediately after application on authority, using their live handle and pawn. Reassigning an already tracked handle to another pawn or ownership is rejected. Avoid engine stacking between participant and pawn lifetimes; read-only export rejects a destination collision instead of merging distinct owners. Abilities derived from the common base automatically mark cooldowns with configurable `CooldownOwnership` (Pawn by default). Instant effects have no active handle: their changes are already represented in attribute bases. Loose tags, standalone cues and game-specific execution data require a domain adapter; cues and granted tags belonging to standard active effects are managed by GAS.

## Shared input and readiness

Both player input and AI tasks call `SubmitAction(ActionTag, bPressed)` on the bound ASC. Matching ability specs use dynamic source action tags. `GetInputSourceTag()` reports `Input.Source.Player` or `Input.Source.AI`; the bound ASC also carries that loose tag. Navigation may continue to use MoveTo through the controller.

Derive game abilities from `UNelaricGameplayAbility` so activation checks the committed binding and initialization readiness. Ability presses and releases forward the active instance's prediction key. Transfer blocks new input and clears held input. Attribute observers should check `IsTransferringState()` before treating temporary cleanup values as damage or death. Input should check `IsReadyForActions()`.

The server replicates the binding owner, revision and commit marker. Clients detach only their own obsolete Avatar binding, wait for replicated AttributeSets, refresh ActorInfo and retry readiness while references arrive. A stale native controller/PlayerState association cannot pass the committed-state gate. Active effects use Full replication for consistent observation of bot and human avatars. Custom AttributeSets must implement their own attribute replication and RepNotify handling.

## Transaction and restoration

Use the [control coordinator](../Player/ControlSwitching.md), including for release without a replacement bot. The GAS participant joins its native export/release/detach/attach/import/validate/commit stages. Both pawns export before either source is cleared. Commit occurs only after imported state, associations and Ready callbacks have passed. A successfully compensated failure leaves the old control relationships and pawn state restored. Failed compensation retains immutable GC-safe exports and reservations, keeping actions blocked.

After repairing native relationships for all affected pawns, call `RestoreReservedState()` on each surviving GAS binding, then `ResolveControlTransitionRecovery()` on the coordinator. Restoration is retry-safe and clears a partial role import first. The coordinator verifies the complete retained batch before committing and releasing it.

Direct Possess or UnPossess on an initialized GAS pawn bypasses this transaction. The binding preserves its old state and rejects readiness under a mismatched context; use the coordinator for ordinary changes. Initial possession can establish a participant before new pawn state is installed. First custody installation waits one tick to allow GameMode's same-frame initial possession. On connection departure the GAS controller attempts a normal handback before native cleanup; if the game policy or state contract rejects it, Unreal's normal pawn departure cleanup applies.

## Domain adapters

Register `Nelaric::GAS::ITransferExtension` instances with stable IDs before control requests. Registration changes are rejected while reserved. Each adapter exports an immutable schema/versioned payload, releases its owned source resources, restores at the selected ASC, verifies restoration and receives a no-fail commit notification. The adapter and its payload are retained during recovery; retain UObject data safely.

Default active ability policy is Cancel. `PreserveExecution`, `DetachExecution` and `FinishExecution`, as well as non-cancelable active abilities, require an extension claiming the spec through `HandlesAbility`. The adapter must quiesce the old execution before common cleanup and implement its actual game semantics; raw UGameplayAbility/UAbilityTask objects are not moved between ASCs. Canceled default executions are not resumed during compensation. Ability specs granted by a participant remain participant-owned, while executions using the departing avatar are ended.

An extension can claim an effect through `HandlesEffect` and perform its restoration instead of the common serializer. Standard restoration inserts an active spec without executing its OnApplied pipeline, restores the remaining duration and schedules the saved periodic deadline. Common periodic restoration supports NeverReset inhibition policy; ResetPeriod and ExecuteAndResetPeriod require an adapter to avoid an extra execution. Effects with custom OnAdded/OnRemoved side effects, special target capture semantics, externally retained handle references, spawned actors or tasks also need an adapter. New runtime effect/spec handles must be rebound by that adapter. This is an explicit domain contract, rather than a promise to serialize arbitrary game objects.

The DemoGame source supplies replicated Health, MaxHealth and Attack attributes, a common PlayerState, a native game mode, and a predicted jump ability reached through `Action.Jump`. The demo GameInstance redirects its legacy map mode to the native GAS mode unless travel explicitly selects a mode; the native mode keeps the existing blueprint character. Other Blueprint game-mode overrides must derive from the GAS game mode and use compatible PlayerState and pawn classes.
