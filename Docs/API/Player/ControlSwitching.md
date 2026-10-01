<!-- Copyright (c) 2026 Nelaric Contributors -->

English | [简体中文](ControlSwitching.zh-CN.md)

# Authority-validated pawn control

`ANelaricPlayerController` sends control intent through its owning connection. The authority validates the request, runs gameplay approval in `ANelaricGameModeBase`, executes possession through `UControlSwitchSubsystem`, and sends the decision back to the requester. Client calls do not possess or unpossess a pawn locally.

## Setup

Use `ANelaricGameModeBase` and its player controller, or derived classes. Add one `UPawnControlComponent` to each eligible pawn. The component derives from `UPawnInitStateComponent`; native components, Blueprint components, and entries in `UPawnInitializationConfig` participate in the existing initialization graph. Avoid adding a second control component when a native base already supplies one.

The component must be registered and Ready on authority before a request can use the pawn. When a controller exists locally, its live PlayerState must match the pawn's PlayerState. Unpossessed pawns and remote AI replicas do not require a local controller. For non-framework pawns, integrate controller and PlayerState changes with the existing initialization coordinator so a replacement context invalidates and retries participants.

`ADemoCharacter` creates a native control component and chooses `ANelaricBotController` as its AI controller class. Existing Blueprint subclasses inherit the component; no binary assets are changed. The framework's generic pawn and character bases remain independent of control policy.

| Setting | Meaning |
| --- | --- |
| `bAllowPlayerControl` | Allows take requests for this pawn. |
| `bAllowReturnControl` | Allows the current participant to release or switch away. |
| `bReturnToBot` | Requires a prepared bot to take over on release; false leaves the pawn unpossessed. |
| `ReturnControllerClass` | Bot class used when the remembered bot is unavailable or busy. |
| `bStartBotLogicOnReady` | Starts registered pawn and controller brain components in an AI-controlled Ready context. |

The default replacement is `ANelaricBotController`, which requests a PlayerState of the game mode's configured class. A custom replacement class must also provide a live PlayerState. Configure brain assets and disable automatic brain startup when readiness is managed by the control component. Bot behavior remains game-defined.

## Player entry points

Call `RequestTakeControl(TargetPawn)` on the owning local player controller. It also switches away from the current pawn when necessary. Call `RequestReturnControl()` to release the authority's current pawn. Both return a positive request ID when sent and zero for invalid local input. Return requests cannot nominate another participant or pawn.

Subscribe to the native `OnControlSwitchDecision()` delegate before sending a request, particularly on standalone and listen-server hosts, where a decision can arrive synchronously. A request ID identifies the action, target, and authority result. This transport does not expose cancellation. Delivery requires a live connection; success reports completed authority possession, not client replication readiness.

The server rejects duplicate or decreasing request IDs and compares the client's expected current pawn with authority state. If a queued request was created for a previous control relationship, it returns `StaleRequest`. A failed attempt consumes its request ID. Reissue after the owning client's pawn state catches up.

## Authority validation and policy

The coordinator checks authority, world teardown, live controllers and PlayerStates, reciprocal possession including the pawn's PlayerState, spectator status, target validity, replicated current and target pawns in networked worlds, component readiness, take/return flags, and existing control. Every controller's PlayerState must be owned by that controller in the same authority world. Each involved pawn must have exactly one control component; ambiguous policies are rejected. Another player's pawn returns `TargetOccupied`. Bots must have a PlayerState; arbitrary non-AI controllers are not displaced.

Override `CanChangePawnControl(Requester, Action, TargetPawn)` in the authority game mode for team, distance, life-state, or permission rules. It is a synchronous predicate: do not possess, unpossess, destroy actors, or start competing control changes inside it. For return requests, `TargetPawn` is the authority's current pawn. The native default approves requests that passed structural and component checks. Structural checks run again after policy callbacks and replacement spawning.

The requester's identity is the controller receiving the owning-connection RPC. No client-supplied requester identity is accepted. Bots and authority gameplay code can call `UControlSwitchSubsystem::ExecuteControlSwitch` with their known controller on the game thread, using the same checks and game policy. A non-Nelaric game mode returns `NotHandled`.

Validation builds a fixed plan with weak references to the requester, current and target pawns, their PlayerStates, the target controller, the authority game mode and an available handback bot. It captures control-component identity, initialization generation, all control settings and the game mode's PlayerState class. A needed replacement class must be concrete and usable before any replacement is spawned. Replacing an identity or changing a captured setting or generation invalidates the plan with `StaleRequest`; the coordinator does not silently approve a new context. Applicability predicates must be read-only.

Reservation checks the complete deduplicated participant set before writing any entries. The current pawn, target pawn, their controllers and PlayerStates, plus an existing selected handback bot and its PlayerState, are reserved together under one transition ID before gameplay approval. A conflict writes no partial reservation. Another request involving a reserved actor returns `ControlTransitionInProgress`, even when possession is temporarily absent or the pawn has become Ready again. Internal validation uses its own transition ID; only that transition can release its reservations.

Preparation runs gameplay approval under these reservations, then revalidates the entire captured plan even when approval is denied. If a new handback bot is required, it is spawned with deferred construction and reserved by pre-spawn initialization before construction, spawn callbacks and BeginPlay. Its initialized PlayerState joins the reservation before possession changes. The plan is checked after spawn callbacks, after finishing construction and immediately before execution. Reused bots remain idle; preparation does not stop the old controller's movement or brain, change possession, replace remembered bots or destroy older cached bots.

Only a completed possession and final context verification update remembered controllers. Denial, preparation failure and successful execution recovery discard an idle bot created by that request, while all reservations remain held during its destruction callbacks. Reused bots are never destroyed by this cleanup. A created controller that game code has adopted to another pawn is left alone. World shutdown invalidates further preparation and reservation attempts.

On authority, call `UPawnControlComponent::IsControlTransitionInProgress()` for its owning pawn, or `UControlSwitchSubsystem::IsControlTransitionInProgress(Actor)` for a pawn, controller or PlayerState. The component returns false on clients; these queries do not replicate state. Readiness, eligibility and active control transition are separate checks.

## Execution and recovery

The authority prepares the current pawn's replacement before changing possession. On take, it stops movement and running brain components in the old contexts, unpossesses the current pawn and target bot, possesses the target with the requester, and hands the old pawn to its prepared bot. On return, it releases the current pawn and applies its configured handback policy. A target already controlled by the requester succeeds without restarting it.

The target's former bot is remembered for later handback only after the operation succeeds. If it has been destroyed or is controlling another pawn, an idle bot previously created for this component can be reused, otherwise a replacement is prepared. The world owns controllers; the component tracks bots it spawned and destroys only idle, unreserved bots when ending its lifecycle. It does not destroy another controller's active pawn or a bot reserved by a different transition.

Initialization context changes for framework pawns are grouped across the operation. Old local bindings are revoked and participants restart with the resulting Controller and PlayerState. Ready callbacks execute before final possession verification. Reservations remain active throughout those callbacks and any restoration callbacks. Game callbacks must not perform competing direct possession changes. Nested requests involving reserved actors return `ControlTransitionInProgress`; unrelated nested coordinator requests still return `Busy` across the world.

Possession is checked in both directions, including the pawn's PlayerState. Failure attempts to restore the former relationships, but never steals an unrelated pawn or controller changed by a callback. `ExecutionFailed` means execution failed and restoration succeeded; `RecoveryFailed` means destruction or a competing callback prevented restoration. Stopped navigation is not resumed automatically. Configured bot brains start again through a Ready control context.

Successful execution, rejection and successful restoration release their own reservations on scope exit. `RecoveryFailed` retains surviving actors' reservations, blocking further ordinary requests. `IsControlTransitionRecoveryRequired(Actor)` distinguishes this state from active execution. After authority gameplay repairs control relationships or removes affected actors, call `ResolveControlTransitionRecovery(Actor)` with any surviving reserved actor. It checks all surviving participants, reciprocal possession, live PlayerStates and reserved pawn readiness before releasing the whole failed transition. It performs no possession and cannot run inside an active control operation. Destroyed participants are pruned before subsequent operations; world teardown clears all records. Reservations use weak references and do not keep actors alive.

## Results and scope

Ordinary rejection does not disconnect the requester. Results include `Denied`, `TargetOccupied`, `NoCurrentPawn`, `PlayerStateUnavailable`, `StaleRequest`, `ReplacementUnavailable`, `ControlTransitionInProgress`, `Busy`, `InvalidRequest`, `InvalidTarget`, and `WorldUnavailable`, in addition to execution and recovery results.

This mechanism updates control relationships and uses each new controller's existing PlayerState. It does not exchange PlayerStates. GameplayRuntime has no GameplayAbilities dependency and does not copy ASC attributes, effects, cooldowns, or running abilities. A GAS integration must coordinate that state transfer with control changes; possession alone is not state migration. Projects that require state continuity must supply that integration before enabling these requests for GAS characters.
