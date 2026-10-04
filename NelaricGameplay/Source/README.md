<!-- Copyright (c) 2026 Nelaric Contributors -->

English | [简体中文](README.zh-CN.md)

# Demo game module

The project's `Source/` directory contains the `DemoGame` Runtime module and
Game, Editor, and Server targets. All three targets build `DemoGame`, which
registers the project's primary game module.

`DemoGame` is the starting point for project-specific demo gameplay.
`ADemoCharacter` derives from the framework's `ANelaricCharacter`, and
`ADemoPlayerCharacter` derives from `ADemoCharacter` for player-controlled
variants. `UDemoPlayerInputComponent` extends the framework's native input
participant; add it to a player character's pawn initialization configuration.
Assign a `UNelaricInputConfig` with `InputTag.Move` (Axis2D),
`InputTag.Look.Mouse` (Axis2D), `InputTag.Look.Stick` (Axis2D), and
`InputTag.Jump` (Boolean) actions and their mapping contexts. The component
binds native movement, camera, and jump callbacks without GAS. These classes
are available to C++ and Blueprint. The module publicly depends on `Core`,
`CoreUObject`, `Engine`, `GameplayRuntime`, `EnhancedInput`, and `GameplayTags`
because its public character and input headers expose framework and engine
types. Add further gameplay classes under
`Public/` and their implementations under `Private/` as needed.

Reusable gameplay contracts and framework actors live in the `GameplayRuntime`
module of the `NelaricGameplay` plugin. The project configuration selects the
plugin's default gameplay classes. See the
[GameplayRuntime module documentation](../../Docs/Modules/Plugins/NelaricGameplay/GameplayRuntime.md)
for its responsibilities and integration contracts.

## Equipment and weapon demo

Equipment lives in `DemoGame/Public/Equipment` and `Private/Equipment`.
`UDemoWeaponDefinition` derives from `UDemoEquipmentDefinition`, and
`UDemoWeaponInstance` derives from `UDemoEquipmentInstance`. Definitions
contain shared authored data; instances contain local identity and lifecycle.
The manager replicates equipment IDs, definitions, and one active equipment ID.
Clients reconstruct instances and cosmetic actors from that snapshot.
Equipment and ammunition remain with their pawn when control changes.
Hitscan combat, finite ammunition, reload and cancellation are implemented
in DemoGame. Inventory and per-equipment ability grants are future features.

Create Blueprint subclasses of `UDemoEquipmentManagerComponent` and
`UDemoPawnAnimationLayerComponent`. Add both classes to the existing
`UPawnInitializationConfig`; do not add components in character constructors.

| Entry | Authority | Client | Replicate Component | Required |
| --- | --- | --- | --- | --- |
| Equipment | true | true | true | true |
| WeaponAnimation | false | true | false | false |

Keep Equipment independent of WeaponAnimation in the dependency graph so
dedicated servers can initialize without animation. Both components implement
the existing init-state participant contract. Only one of each is supported
per pawn. The owning pawn and its initialization coordinator must replicate
for network play. Input components should keep component replication disabled.

Configure the animation component's `MeshComponentName` with the exact pawn
component object name (`CharacterMesh0` for the inherited character mesh).
Optionally set `DefaultAnimationLayer` to an unarmed layer. The main animation
blueprint must contain Linked Anim Layer nodes using a weapon animation layer
interface. Every active weapon layer must implement that interface and use
the same skeleton as the target mesh. This demo uses one weapon animation
channel and one target mesh; first-person and dual-wield profiles are future
extensions. All animation linking runs locally on the game thread.

Create `UDemoWeaponDefinition` Data Assets, set distinct non-empty `Slot`
values, and assign `ActiveAnimationLayer`. The default instance class is
`UDemoWeaponInstance`; a Blueprint subclass may implement lifecycle callbacks.
Optional `Visuals` select a non-replicated cosmetic Actor Blueprint, the exact
mesh component name, socket, and relative transform. Give each visual actor a
root component; collision is disabled for these cosmetic actors. With
`bActiveOnly`, inactive equipment visuals are hidden. Missing meshes or sockets
leave visuals pending. Dedicated servers skip cosmetic actors and animation.

Create a `UDemoEquipmentLoadout` Data Asset with the desired definitions and
`InitialActiveSlot`. Assign it to `InitialLoadout` in the manager Blueprint's
class defaults. The authority applies it once after BeginPlay and component
Ready. Invalid startup entries log a warning and are skipped. Include all
definition, loadout, animation, and visual assets in the cooked game.

Authority-side Blueprint or C++ uses `Equip`, `Unequip`, `ActivateEquipment`,
and `DeactivateEquipment`; these return `EDemoEquipmentResult`. Find an item
by slot, then pass its equipment ID to activation. Failed validation preserves
the current selection. Clients query `GetEquipment` and `GetActiveEquipment`;
the demo does not expose client equip RPCs. Route player requests through the
game's authority-validated input or ability flow. Lifecycle callbacks run on
each machine and reentrant manager mutations return Busy.

## Hitscan combat and Blueprint setup

`ADemoCharacter` grants `UDemoFireAbility` for `Action.Fire` and
`UDemoReloadAbility` for `Action.Reload` through its native state profile.
Custom profiles must include these grants. Ability execution is canceled
by the existing control-transfer flow; per-item ammo stays on the pawn.
The owning client predicts ability lifecycle only. The server decides
shots, damage and ammunition; fire presentation uses an unreliable multicast.

After compiling, restart the editor to load reflected types before editing
assets. Configure the following in the existing Demo content:

1. Create Boolean Input Actions `IA_Character_Fire` and
   `IA_Character_Reload`. Map left mouse and R in `IMC_Character`.
   Add them to the input config's Native Input Actions under
   `InputTag.Character.Fire` and `InputTag.Character.Reload`.
   Use the ordinary Boolean action without a one-shot Pressed trigger for
   fire, so Completed or Canceled occurs when the button is released.
   C++ binds Started, Completed and Canceled; Blueprint input graphs are
   unnecessary. Existing unarmed and primary-weapon actions remain available.
2. Complete the Rifle `UDemoWeaponDefinition`: use `PrimaryWeapon` as Slot,
   assign the compatible active animation layer and cosmetic Visuals, and
   include it in the manager's InitialLoadout. Set InitialActiveSlot to
   `PrimaryWeapon` to start with the rifle active.
3. Keep the native combat defaults or tune them: MagazineCapacity 30,
   InitialReserveAmmo 90, FireInterval 0.1 seconds, Range 10000 centimeters,
   DamagePerShot 10, ReloadDuration 2 seconds, and Automatic enabled.
   The native DamageEffect already reduces Health with the negative
   `Data.Weapon.Damage` magnitude. An additional effect Blueprint is optional.
   A custom effect must be instant and consume the same SetByCaller tag.
4. Make cover and target collision block the definition's TraceChannel,
   which defaults to Visibility. The first implementation targets live,
   active `ADemoCharacter` pawns with committed GAS bindings. Authority traces
   begin at the pawn view location and use its base aim rotation, independently
   of cosmetic weapon meshes. Third-person camera-to-muzzle aiming correction
   and lag compensation are not part of this demo.
5. Optionally assign FireMontage, ReloadMontage and FireSound. Put each
   character montage's Slot in the main animation graph and use a compatible
   skeleton. Reload montage speed and start position follow server time.
   Configure MuzzleSocketName on the cosmetic weapon's skeletal mesh.
   FireGameplayCue defaults to `GameplayCue.Weapon.Rifle.Fire`, and
   ImpactGameplayCue to `GameplayCue.Weapon.Rifle.Impact`. An empty tag disables
   that cue. Create the matching GameplayCueNotify_Static Blueprints below
   `/Game/Demo`, which is already a GameplayCueNotifyPaths discovery root.
   TypeScript mixes OnExecute into the existing cue classes; see below.
   The manager's OnWeaponShot remains available for hit markers and other
   feedback. Do not spawn the same weapon effect there and in a cue.
6. For UI, use GetActiveWeapon, GetWeaponState and GetReloadRemainingTime.
   To observe updates, use a `UDemoWeaponInstance` Blueprint subclass as the
   definition's InstanceClass and implement OnWeaponStateChanged. Initialize
   new widgets by querying current state; snapshots may combine changes.
   On the character Blueprint use GetHealth, GetMaxHealth, OnHealthChanged
   and OnDeath. Death cancels combat, disables movement and stops bot logic;
   Blueprint supplies its animation, UI and return-to-overview behavior.

`TryFire` rejects inactive weapons, dead or parked pawns, reload, empty
magazines and rate-limited requests without changing ammo. An accepted shot
consumes one round even on a miss. `BeginReload` returns a reload GUID;
`CancelReload` accepts only that GUID. Ammo moves only when the matching
authority timer completes, using `min(capacity - magazine, reserve)`.
Control changes, weapon deactivation or removal, initialization revocation,
death, pool parking and teardown cancel pending work. Late reload completions
check both operation identity and the original controller and ASC.

Pool parking preserves health and ammo. For a new life, call the character's
authority-only `ResetCombatState` after GAS is ready; it restores maximum
health, authored weapon ammo and active movement. Returning player control
must not call this reset. `ResetWeaponAmmunition` resets only weapon resources.

Validate misses and hits, empty-magazine rejection, partial reloads, canceled
reloads, control changes while holding fire, death and pool parking. Then run
the same configured content in standalone, listen-server and dedicated-server
sessions. Ammo and health displays must agree with authority, and parked or
inactive items must not retain pending shots or reload completion callbacks.

## Weapon Gameplay Cues

Every accepted shot uses the existing unreliable shot multicast. Rendering
worlds then execute local fire and impact cues with
`ExecuteGameplayCue_NonReplicated`; a dedicated server skips presentation.
Do not execute another replicated cue from OnWeaponShot or from the cue
Blueprint. Damage and ammo remain authority operations, independent of cues.
The two cue events use Executed, with the firing pawn as MyTarget.
An impact fires for any blocking hit, even when no health was reduced.

| Cue parameter | Fire | Impact |
| --- | --- | --- |
| Location | Local muzzle world position | Authority impact world position |
| Normal | Normalized authority shot direction | Authority impact normal |
| TargetAttachComponent | Weapon mesh with the muzzle socket, or null | Null |
| SourceObject | Shot's UDemoWeaponDefinition | Same definition |
| Instigator / EffectCauser | Firing pawn | Firing pawn |
| EffectContext origin | Local muzzle world position | Same muzzle position |
| EffectContext hit result | Authority hit result, including a miss | Blocking hit result |
| PhysicalMaterial | Unset | Hit's physical material, when available |

Resolve the exact firing item by its shot equipment ID, rather than by the
current active slot. If its visual or socket is unavailable, the fire cue
uses the visual origin or authority trace start, with no attach component.
The impact cue still executes when that local item no longer exists.

The existing `GCN_Weapon_Rifle_Fire` and `GCN_Weapon_Rifle_Impact` Blueprints
remain GameplayCueNotify_Static assets below the Demo discovery root.
[`GCN_Weapon_Rifle_Fire_C.ts`](../TypeScript/Demo/GAS/GCN_Weapon_Rifle_Fire_C.ts)
and [`GCN_Weapon_Rifle_Impact_C.ts`](../TypeScript/Demo/GAS/GCN_Weapon_Rifle_Impact_C.ts)
load their respective classes and apply `blueprint.mixin` with objectTakeByNative enabled.
It replaces only OnExecute in place; no generated subclass or reparenting is
required. Do not add a second Niagara graph to OnWeaponShot or K2_HandleGameplayCue.

The fire mixin attaches the imported muzzle system to TargetAttachComponent
using the SourceObject definition's MuzzleSocketName, or spawns at Location
when the attachment is unavailable. It sets User.Direction and User.Trigger
before activation. The impact mixin fills Niagara Position and Vector arrays
with Location and Normal, sets User.NumberOfHits to 1, and selects character
sparks for any Character or subclass hit. Other blocking hits use concrete impact, with
User.StartOffset 0 and User.MuzzlePosition from the context origin.
Every component is created inactive with Auto Destroy enabled and activated
once. A world-owned non-looping Deactivate timer bounds muzzle lifetime to
0.08 seconds and impact lifetime to 1.5 seconds, including imported looping
systems. Timers target components weakly and do not retain a JavaScript
callback. Static cues hold no per-shot components.

[`Entry.ts`](../TypeScript/Entry.ts) imports the mixins. DemoGameInstance Init
acquires the engine-owned script runtime and starts Entry before combat.
Rendering game instances in the same process share one runtime because mixin
modifies shared UClasses. The last game instance's Shutdown releases it;
Puerts restores the original functions when the runtime is destroyed.
Dedicated server game instances skip presentation script startup.
Puerts Auto Mode and the TypeScript editor watcher are separate from this
entry; restarting a play session loads the freshly compiled JavaScript.

Run `npm run typecheck` and `npm run build` from the Unreal project directory.
Generated Puerts declarations and the project tsconfig must be present.
JavaScript output goes to Content/JavaScript. Include that script directory,
the two cue assets, and their Niagara dependencies when packaging.

FireSound and character montages remain optional native presentation.
If a fire cue also plays audio, leave the definition's FireSound empty.
An already accepted burst may finish after a weapon action is canceled;
no persistent Add/Remove cue or additional firing timer is created.
