// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSoldierStateTreeNodes.h Bridges execution to the GameAI Schema. */
#pragma once

#include "AI/GameAIStateTreeNodes.h"
#include "AI/DemoSoldierTypes.h"
#include "StateTreeConditionBase.h"
#include "DemoSoldierStateTreeNodes.generated.h"

class UDemoSoldierComponent;

/// Cheap StateTree outputs copied from the pawn-owned executor.
USTRUCT()
struct FDemoSoldierEvaluatorData : public FGameAIStateTreeContext
{
	GENERATED_BODY()
	/// Observed combat state; no perception queries occur in the evaluator.
	UPROPERTY(EditAnywhere, Category = Output)
	FDemoSoldierMemory Memory;
	/// Accepted intent and its identity, retained across combat interruptions.
	UPROPERTY(EditAnywhere, Category = Output)
	FDemoSoldierOrder Order;
	/// Current intent outcome on the authority executor.
	UPROPERTY(EditAnywhere, Category = Output)
	EDemoSoldierOrderStatus OrderStatus = EDemoSoldierOrderStatus::None;
	/// Current action of the native soldier executor.
	UPROPERTY(EditAnywhere, Category = Output)
	EDemoSoldierBehavior Behavior = EDemoSoldierBehavior::Idle;
	/// Borrowed event source, released when the tree stops.
	UPROPERTY(Transient)
	TWeakObjectPtr<UDemoSoldierComponent> Soldier;
	/// Owned snapshot notification binding, removed on TreeStop.
	FDelegateHandle EventHandle;
};

/// Binds soldier snapshots into an existing GameAI tree without world scans.
USTRUCT(meta = (DisplayName = "Demo Soldier Snapshot"))
struct FDemoSoldierEvaluator : public FGameAIStateTreeEvaluatorBase
{
	GENERATED_BODY()
public:
	/// Instance layout containing the inherited GameAI context and outputs.
	using FInstanceDataType = FDemoSoldierEvaluatorData;

public:
	virtual const UStruct* GetInstanceDataType() const override;
	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void TreeStop(FStateTreeExecutionContext& Context) const override;
};

/// Execution-scoped data; automatically binds the existing GameAI context.
USTRUCT()
struct FDemoSoldierRunTaskData : public FGameAIStateTreeContext
{
	GENERATED_BODY()
	/// Enable only for the legacy single-task native planner integration.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bRunNativePlanner = false;
	/// Initial spawn intent; None, Hold and Defend are supported.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSoldierOrderType InitialOrder = EDemoSoldierOrderType::Hold;
	/// Guard radius in centimeters; sampled once per soldier life.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "1"))
	float InitialHoldRadius = 500.0f;
	/// Borrowed pawn component; populated only after execution is acquired.
	UPROPERTY(Transient)
	TWeakObjectPtr<UDemoSoldierComponent> Soldier;
};

/** @brief Owns the soldier executor for the lifetime of a GameAI tree.
 * @details Use exactly one global task. With native planning disabled,
 * leaf action tasks select behavior. Initial guard intent is applied only
 * once per life and never replaces accepted orders. Stopping the global
 * task cancels actions while retaining intent for control handback.
 */
USTRUCT(meta = (DisplayName = "Run Demo Soldier"))
struct FDemoSoldierRunTask : public FGameAIStateTreeTaskBase
{
	GENERATED_BODY()
public:
	/// Instance layout containing the inherited GameAI context and executor.
	using FInstanceDataType = FDemoSoldierRunTaskData;

public:
	FDemoSoldierRunTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
	                       const FStateTreeTransitionResult& Transition) const override;
};

/// One action request with context bindings and a cancellation identity.
USTRUCT()
struct FDemoSoldierActionTaskData : public FGameAIStateTreeContext
{
	GENERATED_BODY()
	/// Atomic action started on entry and canceled on exit.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSoldierTreeAction Action = EDemoSoldierTreeAction::Idle;
	/// Borrowed executor acquired from the context pawn.
	UPROPERTY(Transient)
	TWeakObjectPtr<UDemoSoldierComponent> Soldier;
	/// Identity prevents stale exits from canceling a later action.
	UPROPERTY(Transient)
	FGuid RequestId;
	/// Owned action completion callback, removed before request cancellation.
	FDelegateHandle FinishedHandle;
};

/** @brief Runs one atomic action selected by the authored soldier tree.
 * @details Requires a global Run Demo Soldier with native planning disabled.
 * Completion callbacks finish the task without Tick polling. Movement,
 * perception and weapon work use events and one-shot deadlines. Reentry
 * starts a new request; exit cancels only this task's owned request.
 */
USTRUCT(meta = (DisplayName = "Demo Soldier Action"))
struct FDemoSoldierActionTask : public FGameAIStateTreeTaskBase
{
	GENERATED_BODY()
public:
	/// Instance layout containing the GameAI context and action identity.
	using FInstanceDataType = FDemoSoldierActionTaskData;

public:
	FDemoSoldierActionTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
	                       const FStateTreeTransitionResult& Transition) const override;
};

/// Context-bound predicate parameters; no output binding is required.
USTRUCT()
struct FDemoSoldierConditionData : public FGameAIStateTreeContext
{
	GENERATED_BODY()
	/// Read-only predicate evaluated against current observed soldier data.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSoldierTreeTest Test = EDemoSoldierTreeTest::Alive;
	/// Negates the result, for example Alive to select the Dead state.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};

/** @brief Tests cheap soldier predicates for entry or root reselection.
 * @details Reads committed memory, item state and action results; never
 * searches for enemies, traces visibility or queries navigation/cover.
 * Missing soldier components always fail, even with inversion enabled.
 */
USTRUCT(meta = (DisplayName = "Demo Soldier Condition"))
struct FDemoSoldierCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()
public:
	/// Instance layout containing automatically bound GameAI context fields.
	using FInstanceDataType = FDemoSoldierConditionData;

public:
	virtual const UStruct* GetInstanceDataType() const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
