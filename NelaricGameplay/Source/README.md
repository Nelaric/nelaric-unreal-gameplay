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
Equipment remains with its pawn when control changes. Combat, ammo, inventory,
and equipment ability grants are separate future features.

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
