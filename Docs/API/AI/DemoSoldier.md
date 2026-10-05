<!-- Copyright (c) 2026 Nelaric Contributors -->

English | [简体中文](DemoSoldier.zh-CN.md)

# Demo soldier AI

The soldier implementation lives in DemoGame, directly on ADemoCharacter. It
uses the existing health, equipment and control lifecycle. GameplayRuntime retains
its genre-independent GameAI contracts. All soldier decisions and mutations run
on the authority game thread. Clients use existing movement, GAS and equipment
replication; combat memory and orders are not replicated by this implementation.

## Setup

Keep the existing Blueprint derived from `ADemoCharacter`; no different pawn
class or reparenting is required. Install UDemoSoldierComponent through its
UPawnInitializationConfig DA. The component implements the existing InitState
contract. ADemoCharacter does not create Soldier or a fallback PawnControl and
does not override the configured return controller. GetSoldierComponent queries
the installed component and returns null before creation or after removal.

Create a component Blueprint derived from UDemoSoldierComponent to author
Settings, and select that class in the DA. A native component class also works
with its defaults. A minimal authority-only entry is:

| Entry field | Value |
| --- | --- |
| ComponentId | Soldier, or another stable unique ID |
| ComponentClass | UDemoSoldierComponent or its component Blueprint |
| bCreateOnAuthority | true |
| bCreateOnClient | false |
| bReplicateComponent | false |
| bRequiredForPawnReady | true |
| DependencyIds | Actual IDs of required equipment/control entries in this DA |

Only name IDs that exist in the DA. Entries created on clients cannot depend on
this authority-only entry. Keep PawnControl in the same initialization DA and
author its ReturnControllerClass as ADemoSoldierController or the matching
controller Blueprint. In the character Blueprint, set AIControllerClass to that
controller, AutoPossessAI to PlacedInWorldOrSpawned, and enable crouching. Keep the
existing main animation and initial equipment, and supply a built navigation mesh.
Controller-owned native brain and perception components remain internal to
ADemoSoldierController; they are not pawn initialization DA entries.

PawnControl starts the brain after its Ready group commits; Soldier also retries
that policy through a one-shot callback on full pawn Ready. Pool prewarm disables
AutoPossessAI. After activation, Soldier creates the character Blueprint's
AIControllerClass for an active unpossessed character and uses the existing
ControlSwitchSubsystem for initial possession and GAS state transfer. PawnControl
eligibility and GameMode control policy still apply; direct Possess is invalid
after initialization. The
new full pawn Ready notification starts the brain after possession initialization.
This does not take control from players, create bots during prewarm, or poll for
readiness. Execution requires the entire
pawn, Soldier and GAS to be Ready. Pawn revocation stops execution immediately.
Player possession stops
execution and bot handback resumes the same pawn-owned intent. No StateTree asset
is required for native execution. Query the component after initialization and
check for null before issuing an order; it is absent on clients with this setup.

Team identity is injected externally and stored on ADemoCharacter as a replicated
actor property. Soldier components and initialization DAs do not configure TeamId;
the component GetTeamId reads its owner. Equal IDs are friendly, different IDs
are hostile, and 255 is neutral. Unassigned characters start with 255.

The first injection point is ADemoInitialCharacterSpawnPoint.TeamId. Select each
initial marker in the level and set Team Id under Demo / Spawning, for example
0 and 1. The marker passes its ID to Pool->TryAcquire(Transform, TeamId); the demo
pool policy calls Character->SetTeamId before activation enables collision,
movement or an existing brain. All teams share the same BP_DemoCharacter, soldier
component Blueprint and initialization DA. Markers default to team 0. Return
stops the character before clearing its identity; every acquisition reassigns
the team, with an omitted argument selecting a neutral character.

Other spawning flows call DemoCharacter.SetTeamId on the authority game thread,
even before the Soldier component exists. Read the identity with GetTeamId.
DemoCharacter implements UE GenericTeamAgentInterface, so its team remains
visible without a Soldier component. Player possession and bot handback retain
the character identity. Reassignment updates the AIController, refreshes soldier
hostility, and notifies soldiers currently seeing that character through event
callbacks without adding Tick. Override IsHostile for other faction policies.

## Orders and interruptions

```cpp
FDemoSoldierOrder Order;
Order.Type = EDemoSoldierOrderType::Move;
Order.Location = Destination;
Order.AcceptanceRadius = 100.0f;
Soldier->IssueOrder(Order);
```

IssueOrder validates data, assigns an identity if absent, and replaces previous
intent. Type None cancels intent. Read GetOrder and GetOrderStatus after acceptance.
Move completes on arrival. Hold and Defend remain active and constrain pursuit,
investigation and cover destinations to HoldRadius. Follow requires a live actor
and refreshes its destination. Attack without an actor advances to a position and
completes on arrival. Attack with an actor advances to the position supplied by the
order and attacks when observed; observed death completes it, while destroyed
unobserved identities fail. Attack goals are separate from selected combat targets.

Combat, investigation and emergencies preserve the current order. Completion
resumes intent from the pawn's current location. Grenade escape may temporarily
leave a guard area; the soldier waits until the danger report expires or is cleared
and then returns. Movement has a timeout; failures terminate the affected order.

## Combat and observations

Sight records bounded weak contacts; hearing and damage never grant visibility.
Visible contacts may refresh their positions. Lost contacts retain their last
observed position and expire. Target selection combines visibility, recent damage,
distance and assigned-target preference, with lock time and a score margin.

The native executor schedules reaction, finite bursts and observation pauses.
Weapon intervals, traces, ammunition and reload remain owned by DemoWeaponInstance.
An additional shot-time trace checks obstruction before requesting a shot. Reload
uses its existing identity and cancellation result. Low magazines reload during a
safe window; empty magazines reload when reserves exist. Empty reserves have an
explicit OutOfAmmo action. Grenades preempt any action; cover movement and reload
finish before normal combat is reconsidered.

Accepted DemoWeaponInstance shots report a Gunshot hearing stimulus. Committed
negative GAS health effects report damage with a directional cue. External UE
damage-sense stimuli are also accepted; do not report the same damage through both
paths. Other noise producers can use UE ReportNoiseEvent or ReportSound directly.

ReportNearMiss adds decaying suppression. ReportGrenade accepts an observed center,
radius and remaining lifetime; it does not read the source actor's transform.
ClearGrenade removes a disarmed or canceled report. Reports, contacts and retries
are bounded. Escape and cover queries run on action entry or after failure, rather
than every frame. Cover currently uses authored ADemoSoldierCoverPoint markers,
navigation reachability and a blocked crouching sightline. Set each marker at floor
height behind blocking collision. Dynamic cover generation is not included.

## Authored StateTree setup

The authored tree selects atomic actions; the component executes only the selected
action and maintains observed memory. The legacy native planner remains available
through Run Native Planner, but disable it for the tree below. Use one execution
driver, exactly one global Run Demo Soldier, and one Demo Soldier Action per leaf.

1. Create BP_DemoSoldierController derived from ADemoSoldierController. Disable
   Use Native Brain in Class Defaults. Add Game AI State Tree Component and disable
   Start Logic Automatically. The existing pawn Ready flow starts the brain.
2. Create ST_DemoSoldier using the Game AI schema (UGameAIStateTreeSchema),
   and assign it to the controller's component. Do not use the generic AI schema.
3. Keep BP_DemoCharacter and its DA-created Soldier, equipment and PawnControl.
   Set both PawnControl.ReturnControllerClass and the character AIControllerClass
   to this controller Blueprint. Use PlacedInWorldOrSpawned AutoPossessAI. Do not
   add a duplicate Soldier component in the character Blueprint.
4. Add Run Demo Soldier under Global Tasks: Run Native Planner=false,
   Initial Order=Hold, Initial Hold Radius=500. First execution creates a guard
   intent at the spawn position without a level Blueprint. None starts without
   intent. Apply initial intent once per life; existing, completed and canceled
   intent is preserved, including across control handback.
5. Optionally add Demo Soldier Snapshot under Evaluators for Memory, Order,
   OrderStatus and Behavior debugging. Conditions read committed state directly.
   The GameAI context fields are automatically bound; no snapshot binding is needed.

### Hierarchy

Keep the following sibling order. Containers use Try Select Children In Order;
leaves use Try Enter. Containers have no action tasks.

```text
Root
├─ Dead
├─ Emergency
│  └─ AvoidGrenade
├─ Combat
│  ├─ Reload
│  ├─ TakeCover
│  ├─ OutOfAmmo
│  ├─ TargetVisible
│  │  ├─ ApproachTarget
│  │  ├─ Aim
│  │  ├─ FireBurst
│  │  └─ Reevaluate
│  └─ TargetLost
│     ├─ MoveLastKnownPosition
│     ├─ Search
│     └─ ForgetTarget
├─ Alert
│  ├─ InvestigateDamage
│  └─ InvestigateSound
├─ ExecuteOrder
│  ├─ ReturnToArea
│  ├─ Move
│  ├─ Hold
│  ├─ Attack
│  ├─ Follow
│  └─ Defend
└─ Idle
```

### Conditions and tasks

Test names below are parameters of Demo Soldier Condition. Enable Invert only
where stated. Action names are parameters of Demo Soldier Action. A dash means
no condition or task is needed.

| State | Entry Test | Leaf Action |
| --- | --- | --- |
| Root | — | — |
| Dead | Alive, Invert=true | Dead |
| Emergency | Grenade | — |
| AvoidGrenade | — | AvoidGrenade |
| Combat | Combat | — |
| Reload | NeedsReload | Reload |
| TakeCover | NeedsCover | TakeCover |
| OutOfAmmo | OutOfAmmo | OutOfAmmo |
| TargetVisible | TargetVisible | — |
| ApproachTarget | CanApproach | ApproachTarget |
| Aim | — | Aim |
| FireBurst | — | FireBurst |
| Reevaluate | — | Observe |
| TargetLost | TargetVisible, Invert=true | — |
| MoveLastKnownPosition | CanSearchMove | MoveToMemory |
| Search | — | Search |
| ForgetTarget | — | ForgetTarget |
| Alert | Alert | — |
| InvestigateDamage | DamageCue | InvestigateDamage |
| InvestigateSound | — | InvestigateSound |
| ExecuteOrder | HasOrder | — |
| ReturnToArea | ReturnToArea | ExecuteOrder |
| Move | MoveOrder | ExecuteOrder |
| Hold | HoldOrder | ExecuteOrder |
| Attack | AttackOrder | ExecuteOrder |
| Follow | FollowOrder | ExecuteOrder |
| Defend | DefendOrder | ExecuteOrder |
| Idle | — | Idle |

Combat includes valid combat targets and eligible weapon/cover upkeep. NeedsReload
handles empty magazines and safe low-ammo windows; no reserve means no reload.
Failed cover queries enter a retry cooldown, allowing combat without CoverPoints.
OutOfAmmo requires a visible target; lost targets still receive a search. Grenade
escape may cross guard boundaries, then ReturnToArea restores the retained area.

### Transitions

Add these two Root transitions; sibling ordering alone does not interrupt a
running action. Use only the listed conditions and leave event delay disabled.
Leave Root Enter Conditions empty. Add ShouldReconsider to the first transition's
own Conditions. It decides whether to interrupt an action, not whether Root may
be entered.

| Trigger | Required Event → Tag | Condition | Target | Priority |
| --- | --- | --- | --- | --- |
| On Event | AI.Event.DecisionChanged | Demo Soldier Condition: ShouldReconsider | Root | High |
| On State Completed | — | — | Root | Not applicable (hidden in the editor) |

Do not add an On Tick transition. ShouldReconsider runs on committed change events
and checks orders, grenade danger, target changes and action categories. Leave the
event payload empty and use default event consumption. The component sends the
event after committing observations/actions; Blueprint event forwarding is not
required. Root completion is a fallback for action success or failure. UE 5.6
searches completion transitions from the completed state up through its parents
and stops at the first transition whose conditions pass and whose target can be
selected. Explicit leaf transitions therefore run before the Root fallback.
On State Completed, On State Succeeded and On State Failed do not expose
configurable Priority or Delay; no priority setup is required for these triggers.

Demo Soldier Action disables Task Tick and finishes through request-scoped
callbacks and StateTree FinishTask. Snapshot updates through notifications without
an Evaluator Tick override. The authored path does not poll DecisionInterval:
perception, committed equipment/ammo state, visible actor transforms and health,
movement results and actor end play wake execution. Self movement callbacks wake
only for relevant guard, danger, range or cover boundaries; rotation callbacks
handle pending aim alignment only.

Reaction, burst cadence, observation, search, memory/danger expiry and timeouts use
one-shot deadline callbacks. Schedule the nearest known deadline, handle it, then
schedule the next deadline. No fixed decision loop runs; without events or pending
deadlines, no component update is scheduled. Sample action parameters on entry.

| Source leaf | Trigger | Target |
| --- | --- | --- |
| Aim | On State Succeeded | FireBurst |
| Aim | On State Failed | Reevaluate |
| FireBurst | On State Completed | Reevaluate |
| Reevaluate | On State Completed | Root |
| ApproachTarget | On State Succeeded | Aim |
| ApproachTarget | On State Failed | Reevaluate |
| MoveLastKnownPosition | On State Completed | Search |
| Search | On State Succeeded | ForgetTarget |
| ForgetTarget | On State Completed | Root |
| Reload | On State Completed | Root |
| TakeCover | On State Completed | Root |
| AvoidGrenade | On State Completed | Root |
| InvestigateDamage | On State Completed | Root |
| InvestigateSound | On State Completed | Root |
| ReturnToArea, Move, Hold, Attack, Follow, Defend | On State Completed | Root |

Idle, OutOfAmmo and Dead need no completion transition. Idle, Hold, Follow, Defend
and OutOfAmmo may remain Running until root reselection. Aim is the visible-target
default; FireBurst and Reevaluate are reached by transitions, not by sequential
sibling execution.

### Diagnosing inactive units

Inspect the authority controller and StateTree debug instance. A successful start
logs Soldier execution started with the pawn, controller, driver, planner mode and
team. Controller creation failure, a missing Game AI State Tree Component, and
global/action task startup failures log their configuration requirements. Enable
Log LogDemoSoldier Verbose in the Output Log console for readiness prerequisites.

Initial Hold guards the spawn location, so standing without a hostile contact is
expected. Set different Team Id values on the initial markers, such as 0 and 1;
both markers share the same character Blueprint and initialization DA. Verify
the team field in Initial character spawned logs or query the character GetTeamId. The running instance should remain at Root / ExecuteOrder /
Hold before contact, then select Combat and cycle Aim, FireBurst and Reevaluate.
Select On State Succeeded explicitly for Aim and ApproachTarget success transitions,
and On State Failed for failures, instead of the new transition's default On State
Completed. Search succeeds into ForgetTarget. Completion transitions need no
Priority setting: a valid leaf completion transition runs before the search
reaches the parent Root fallback.

Do not add unconditional Root transitions for every AI.Event: ordinary perception
changes must not repeatedly cancel reload or cover movement. ShouldReconsider
handles interrupt restrictions. Leaf exits cancel owned requests and retain Order.
Burst counts are generated once. Failed memory movement still proceeds to a brief
search, then forgetting. Character death stops the brain through the existing
lifecycle; Dead is a terminal fallback. Existing character code handles presentation.

### Asset verification

Compile and save the controller Blueprint and StateTree. In a navigable test level,
place two initial character markers with Team Id 0 and 1 to activate existing
BP_DemoCharacter instances from the pool.
They guard their spawn area before contact, cycle Aim → FireBurst → Reevaluate
with visual contact, and search the last known location after occlusion before
returning to their retained area. Inspect the active leaf in StateTree Debugger
and the retained Order identity in Snapshot, including after control handback.
PIE with the authored asset is required to verify these behaviors.

## Lifecycle

Events use native AI.Event tags and reach OnEvent listeners through a deferred
one-shot callback after mutations complete. The StateTree bridge also forwards
those tags to its executing component. DecisionChanged follows committed memory
and action changes. These are change notifications: query current snapshots rather
than interpreting them as historical action results. Notifications are bounded and
coalesced by tag. Movement and reload callbacks match their request identities;
old callbacks cannot complete a replacement action. Leaf exits unbind completion
callbacks before canceling their owned requests. Stopping execution removes
transform, health, equipment and end-play observers and clears pending deadlines.

Stopping the brain cancels movement, finite fire and owned reload, removes action
bindings, restores movement settings, and retains intent for control handback.
InitState invalidation cancels the same actions and queued events. Removing or
replacing the DA entry destroys the component and its intent; the character getter
does not retain a stale instance.
Death and pool deactivation use the existing brain-stop paths. ResetCombatState
also clears soldier intent and observations for a new life. Pool reuse that should
represent a different soldier must explicitly call ResetSoldierState or
ResetCombatState; ordinary activation preserves retained state.
