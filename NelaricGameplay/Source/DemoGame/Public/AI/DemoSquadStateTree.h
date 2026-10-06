// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSquadStateTree.h Declares atomic squad nodes for GameAI. */
#pragma once

#include "AI/DemoSquadTypes.h"
#include "AI/GameAIStateTreeNodes.h"
#include "StateTreeConditionBase.h"
#include "DemoSquadStateTree.generated.h"

class UDemoSquadContextComponent;

/// Shared GameAI context; squad data is resolved from its owning actor.
USTRUCT()
struct FDemoSquadTreeContext
{
	GENERATED_BODY()
	/// Actor hosting the shared GameAI component.
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> OwnerActor = nullptr;
	/// Executing component; no possession context is consumed.
	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<UStateTreeComponent> StateTreeComponent = nullptr;
};

/** @brief Acquires squad data updates for the lifetime of the tree.
 * @details Use one Global Task. This task owns no tactical policy.
 * Exit releases plan resources and game-thread subscriptions.
 */
USTRUCT(meta = (DisplayName = "Run Demo Squad"))
struct FDemoSquadRunTask : public FGameAIStateTreeTaskBase
{
	GENERATED_BODY()
public:
	/// Named GameAI context supplied by the executing component.
	using FInstanceDataType = FDemoSquadTreeContext;

public:
	FDemoSquadRunTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
	                       const FStateTreeTransitionResult& Transition) const override;
};

/// One atomic action selected by an authored state.
UENUM()
enum class EDemoSquadTreeOperation : uint8
{
	BuildPlan,       ///< Await planning.
	Execute,         ///< Await phase feedback.
	Leadership,      ///< Confirm succession.
	CompletePlan,    ///< Finish the plan.
	CompleteMission, ///< Finish the objective.
	FailMission,     ///< Fail the objective.
	SetCommandMode,  ///< Apply command policy.
	DegradeOrders,   ///< Retain local holds.
	Cleanup,         ///< Release resources.
};

/// Authored task inputs and one owned completion subscription.
USTRUCT()
struct FDemoSquadTreeTaskData : public FDemoSquadTreeContext
{
	GENERATED_BODY()
	/// Atomic operation performed on entry.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadTreeOperation Operation = EDemoSquadTreeOperation::Execute;
	/// Plan label selected in the tree, without native tactic inference.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadTactic Tactic = EDemoSquadTactic::Move;
	/// Goal data source explicitly selected by the planning state.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadGoalSource GoalSource = EDemoSquadGoalSource::Mission;
	/// Whether this plan allocates support, movement and coordination groups.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bSplitSupport = false;
	/// Maximum eligible support participants for a split plan.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "1", ClampMax = "32"))
	int32 SupportGroupSize = 3;
	/// Explicit member orders, constraints, deadline and readiness rule.
	UPROPERTY(EditAnywhere, Category = Parameter)
	FDemoSquadPhaseSettings Settings;
	/// Policy used only by SetCommandMode.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadCommandMode CommandMode = EDemoSquadCommandMode::PlayerAssisted;
	/// Reason used only by FailMission.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadFailure Failure = EDemoSquadFailure::PhaseTimeout;
	/// Captured identity rejects results belonging to another plan.
	FDemoSquadRequestIdentity Identity;
	/// Owned context subscription removed on state exit.
	FDelegateHandle ChangedHandle;
};

/** @brief Executes one authored action and reports its own outcome.
 * @details Build and Execute await committed context notifications.
 * Phase exit removes only this task's subscription; support belongs to
 * the plan. No node chooses tactics or advances to another phase.
 */
USTRUCT(meta = (DisplayName = "Demo Squad Operation"))
struct FDemoSquadTreeTask : public FGameAIStateTreeTaskBase
{
	GENERATED_BODY()
public:
	/// Context and authored atomic-action inputs.
	using FInstanceDataType = FDemoSquadTreeTaskData;

public:
	FDemoSquadTreeTask();
	virtual const UStruct* GetInstanceDataType() const override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context,
	                       const FStateTreeTransitionResult& Transition) const override;
};

/// Raw predicates available to authored entry conditions and transitions.
UENUM()
enum class EDemoSquadTreeTest : uint8
{
	LeadershipReady,        ///< Commander confirmed.
	LeaderPlayerControlled, ///< Player drives commander.
	HasMembers,             ///< Living roster exists.
	HasMission,             ///< Objective active.
	MissionType,            ///< Objective type matches.
	MissionExpired,         ///< Objective expired.
	HasFallback,            ///< Fallback exists.
	CommandMode,            ///< Command policy matches.
	ManualTactic,           ///< Manual tactic matches.
	Engagement,             ///< Firing policy matches.
	PlanCurrent,            ///< Plan authority matches.
	Phase,                  ///< Phase label matches.
	RosterReady,            ///< Captured roster Ready.
	MetricAtLeast,          ///< Snapshot threshold met.
};

/// Snapshot measurements with thresholds authored in the tree.
UENUM()
enum class EDemoSquadTreeMetric : uint8
{
	EffectiveMembers, ///< Automatic participants.
	MobileMembers,    ///< Mobile participants.
	ReadySupport,     ///< Ready support count.
	AmmoReadiness,    ///< Armed fraction.
	ThreatPressure,   ///< Observed pressure.
	SuppressionRatio, ///< Suppressed fraction.
	PhaseAge,         ///< Elapsed phase seconds.
	PlanAge,          ///< Elapsed plan seconds.
};

/// Raw comparison inputs, independent of tactical policy.
USTRUCT()
struct FDemoSquadTreeConditionData : public FDemoSquadTreeContext
{
	GENERATED_BODY()
	/// Comparison performed on the latest committed snapshot.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadTreeTest Test = EDemoSquadTreeTest::LeadershipReady;
	/// Tactic used by ManualTactic.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadTactic Tactic = EDemoSquadTactic::Idle;
	/// Phase label used by Phase.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadPhase Phase = EDemoSquadPhase::None;
	/// Objective type used by MissionType.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadMissionType MissionType = EDemoSquadMissionType::None;
	/// Control policy used by CommandMode.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadCommandMode CommandMode = EDemoSquadCommandMode::Autonomous;
	/// Firing policy used by Engagement.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadEngagement Engagement = EDemoSquadEngagement::FireAtWill;
	/// Snapshot field used by MetricAtLeast.
	UPROPERTY(EditAnywhere, Category = Parameter)
	EDemoSquadTreeMetric Metric = EDemoSquadTreeMetric::ThreatPressure;
	/// Inclusive minimum; tree conditions can negate it for an upper bound.
	UPROPERTY(EditAnywhere, Category = Parameter)
	float Threshold = 0.15f;
	/// Missing context fails even when inversion is enabled.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bInvert = false;
};

/// Cheap raw predicates; selection priority and transitions live in assets.
USTRUCT(meta = (DisplayName = "Demo Squad Condition"))
struct FDemoSquadTreeCondition : public FStateTreeConditionCommonBase
{
	GENERATED_BODY()
public:
	/// GameAI context and explicit comparison inputs.
	using FInstanceDataType = FDemoSquadTreeConditionData;

public:
	virtual const UStruct* GetInstanceDataType() const override;
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;
};
