<!-- Copyright (c) 2026 Nelaric Contributors -->

English | [简体中文](README.zh-CN.md)

# Nelaric Unreal Gameplay API

Nelaric Unreal Gameplay is a gameplay framework for Unreal Engine 5.6 and later. Its public contracts support gameplay development across standalone, listen-server, and dedicated-server topologies.

Public APIs follow the [project coding standards](../CodingStandards/README.md), including the rules for gameplay extension contracts, ownership, errors, and documentation.

## Design

- [Network sessions and authority transitions](Core/NetWork/NetworkSessionTransitions.md)
- [Native input](Input/NativeInput.md)
- [Authority-validated pawn control](Player/ControlSwitching.md)
- [GAS state across control changes](GAS/ControlState.md)
- [Fixed-capacity native object pools](ObjectPool/FixedObjectPool.md)

## GameAI StateTree

DemoGame provides an optional [soldier AI example](AI/DemoSoldier.md) using the
existing character, weapon and control lifecycle, including native execution and
GameAI StateTree bridge nodes.

The [company command implementation (Chinese)](AI/DemoCompany.zh-CN.md) provides one
authorized publisher per runtime world, concurrent objectives, scoped assignments,
execution permissions, world-owned objective rules, player scopes and save reconciliation.
The [virtual platoon implementation (Chinese)](AI/DemoPlatoon.zh-CN.md) coordinates
configured virtual squads through six mission templates and explicit readiness.

Use the existing `GameplayRuntime` module dependency and include headers from its `AI/` directory. Create a StateTree asset with the `Game AI` Schema (`UGameAIStateTreeSchema`) and run it using `UGameAIStateTreeComponent` on a pawn or controller. Set the inherited Context Actor Class to the actual component owner's type. The required context entries are:

| Entry | Type | Source |
| --- | --- | --- |
| OwnerActor | `AActor` or the configured subclass | Component's actual owner |
| Pawn | `APawn` | Owning pawn or controller's possessed pawn |
| Controller | `AController` | Owning controller or pawn's current controller |
| GameContext | `UGameAIContextSubsystem` or the selected native subclass | Executing world's subsystem collection |
| StateTreeComponent | `UStateTreeComponent` | Executing component itself |

Native tasks and evaluators derive from `FGameAIStateTreeTaskBase` and `FGameAIStateTreeEvaluatorBase`. Their default instance data is `FGameAIStateTreeContext`; access it with `Context.GetInstanceData` during node callbacks, then use `.Pawn`, `.Controller`, or `.GameContext`. To add instance fields, derive a reflected struct from the context struct, alias it as `FInstanceDataType`, and override `GetInstanceDataType()` to return that struct's `StaticStruct()`.

Blueprint nodes derive from `UGameAIStateTreeTaskBlueprintBase` or `UGameAIStateTreeEvaluatorBlueprintBase` and read the inherited context properties directly. The StateTree compiler binds fields in the `Context` category by compatible type and property name. The Schema describes data and authoring policy; it does not add fields to `FStateTreeExecutionContext`.

The Schema admits these task/evaluator families, common native conditions, considerations and property functions, and Blueprint conditions. Other task/evaluator families and arbitrary external-data linking are denied; use the named context properties for injection. Existing generic or AI tasks require an adapter deriving from the GameAI task base.

All five entries must exist before execution. If possession occurs after BeginPlay, disable automatic startup and call `StartLogic()` after possession. Stop logic before unpossession or owner teardown changes the required context, and restart it once the context is ready. Context references are local, game-thread data; revalidate references retained outside a callback. Native context subsystem subclasses may add services; choose their class on the Schema. The base subsystem remains a world-owned empty extension point and can coexist with native subclass instances.
