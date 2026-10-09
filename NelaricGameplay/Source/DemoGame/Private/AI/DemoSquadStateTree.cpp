// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSquadStateTree.h"
#include "AI/DemoSquadCommandActor.h"
#include "AI/DemoSquadContextComponent.h"
#include "Components/StateTreeComponent.h"
#include "Engine/World.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeAsyncExecutionContext.h"

namespace Nelaric::Squad
{
static UDemoSquadContextComponent* ResolveContext(const FDemoSquadTreeContext& Data)
{
	const auto* Actor = Cast<ADemoSquadCommandActor>(Data.OwnerActor);
	return IsValid(Actor) && Actor->HasAuthority() ? Actor->GetSquadContext() : nullptr;
}

static EStateTreeRunStatus OperationResult(const FDemoSquadTreeTaskData& Data)
{
	auto* Squad = ResolveContext(Data);
	if (!IsValid(Squad))
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.Operation == EDemoSquadTreeOperation::Leadership)
	{
		return Squad->ResolveLeadership() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
	}
	if (Data.Operation == EDemoSquadTreeOperation::Execute)
	{
		Squad->MaintainPhase();
	}
	const auto Plan = Squad->GetPlan();
	if (Plan.Phase == EDemoSquadPhase::Failed || !Squad->IsCurrentIdentity(Data.Identity))
	{
		return EStateTreeRunStatus::Failed;
	}
	if (Data.Operation == EDemoSquadTreeOperation::BuildPlan)
	{
		return Plan.Phase != EDemoSquadPhase::Planning ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
	}
	const auto Mission = Squad->GetMission();
	if (Data.Operation == EDemoSquadTreeOperation::Execute && Squad->UsesMemberOrderPolicy())
		return EStateTreeRunStatus::Running;
	if (Data.Operation == EDemoSquadTreeOperation::Execute &&
	    ((Mission.Type == EDemoSquadMissionType::Defend && Plan.Phase == EDemoSquadPhase::Maintain) ||
	     (Mission.Type == EDemoSquadMissionType::Control &&
	      Squad->GetMissionResult().State == EDemoSquadMissionState::Succeeded)))
		return EStateTreeRunStatus::Running;
	return Squad->IsPhaseReady() ? EStateTreeRunStatus::Succeeded : EStateTreeRunStatus::Running;
}
} // namespace Nelaric::Squad

FDemoSquadRunTask::FDemoSquadRunTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = false;
#if WITH_EDITORONLY_DATA
	bConsideredForCompletion = false;
	bCanEditConsideredForCompletion = false;
#endif
}
const UStruct* FDemoSquadRunTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}
EStateTreeRunStatus FDemoSquadRunTask::EnterState(FStateTreeExecutionContext& Context,
                                                  const FStateTreeTransitionResult& Transition) const
{
	const auto& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	auto* Squad = Nelaric::Squad::ResolveContext(Data);
	return Squad && Squad->StartExecution(Data.StateTreeComponent) ? EStateTreeRunStatus::Running
	                                                               : EStateTreeRunStatus::Failed;
}
void FDemoSquadRunTask::ExitState(FStateTreeExecutionContext& Context,
                                  const FStateTreeTransitionResult& Transition) const
{
	const auto& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	if (auto* Squad = Nelaric::Squad::ResolveContext(Data))
	{
		Squad->StopExecution(Data.StateTreeComponent);
	}
}
FDemoSquadTreeTask::FDemoSquadTreeTask()
{
	bShouldCallTick = false;
}
const UStruct* FDemoSquadTreeTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}
EStateTreeRunStatus FDemoSquadTreeTask::EnterState(FStateTreeExecutionContext& Context,
                                                   const FStateTreeTransitionResult& Transition) const
{
	auto& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	auto* Squad = Nelaric::Squad::ResolveContext(Data);
	if (!Squad || !Data.StateTreeComponent)
	{
		return EStateTreeRunStatus::Failed;
	}
	switch (Data.Operation)
	{
	case EDemoSquadTreeOperation::Cleanup:
		Squad->CleanupSquad();
		return EStateTreeRunStatus::Succeeded;
	case EDemoSquadTreeOperation::DegradeOrders:
		Squad->DegradeOrders();
		return EStateTreeRunStatus::Succeeded;
	case EDemoSquadTreeOperation::CompletePlan:
		Squad->CompletePlan();
		return EStateTreeRunStatus::Succeeded;
	case EDemoSquadTreeOperation::CompleteMission:
	{
		const auto Mission = Squad->GetMission();
		return Squad->ConfirmMissionCompleted(Mission.MissionId, Mission.Revision) ? EStateTreeRunStatus::Succeeded
		                                                                           : EStateTreeRunStatus::Failed;
	}
	case EDemoSquadTreeOperation::FailMission:
		Squad->ReportMissionFailure(Data.Failure);
		return EStateTreeRunStatus::Succeeded;
	case EDemoSquadTreeOperation::SetCommandMode:
		Squad->SetCommandMode(Data.CommandMode);
		return EStateTreeRunStatus::Succeeded;
	case EDemoSquadTreeOperation::BuildPlan:
		if (!Squad->BuildPlan(Data.Tactic, Data.GoalSource, Data.bSplitSupport, Data.SupportGroupSize))
		{
			return EStateTreeRunStatus::Failed;
		}
		break;
	case EDemoSquadTreeOperation::Execute:
		if (!Squad->BeginPhase(Data.Settings))
		{
			return EStateTreeRunStatus::Failed;
		}
		break;
	default:
		break;
	}
	Data.Identity = Squad->GetPlan().Identity;
	Data.ChangedHandle = Squad->OnChanged().AddLambda(
	    [WeakContext = Context.MakeWeakExecutionContext()]()
	    {
		    auto Strong = WeakContext.MakeStrongExecutionContext();
		    if (auto* Live = Strong.GetInstanceDataPtr<FDemoSquadTreeTaskData>())
		    {
			    const auto Result = Nelaric::Squad::OperationResult(*Live);
			    if (Result != EStateTreeRunStatus::Running)
			    {
				    WeakContext.FinishTask(Result == EStateTreeRunStatus::Succeeded
				                               ? EStateTreeFinishTaskType::Succeeded
				                               : EStateTreeFinishTaskType::Failed);
			    }
		    }
	    });
	return Nelaric::Squad::OperationResult(Data);
}
void FDemoSquadTreeTask::ExitState(FStateTreeExecutionContext& Context,
                                   const FStateTreeTransitionResult& Transition) const
{
	auto& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	if (auto* Squad = Nelaric::Squad::ResolveContext(Data))
	{
		Squad->OnChanged().Remove(Data.ChangedHandle);
	}
	Data.ChangedHandle.Reset();
}
const UStruct* FDemoSquadTreeCondition::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}

bool FDemoSquadTreeCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const auto& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	const auto* Squad = Nelaric::Squad::ResolveContext(Data);
	if (!Squad)
	{
		return false;
	}
	const auto Plan = Squad->GetPlan();
	const auto Mission = Squad->GetMission();
	bool Result = false;
	switch (Data.Test)
	{
	case EDemoSquadTreeTest::LeadershipReady:
		Result = Squad->IsLeadershipReady();
		break;
	case EDemoSquadTreeTest::LeaderPlayerControlled:
		Result = false; // Compatibility with old assets; command has no body.
		break;
	case EDemoSquadTreeTest::HasMembers:
		Result = Squad->GetMembers().ContainsByPredicate([](const auto& Member) { return Member.bAlive; });
		break;
	case EDemoSquadTreeTest::HasMission:
		Result = Squad->IsExecutionPermitted() &&
		         (Squad->GetMissionResult().State == EDemoSquadMissionState::Running ||
		          (Squad->GetMissionResult().State == EDemoSquadMissionState::Succeeded &&
		           (Mission.Type == EDemoSquadMissionType::Defend || Mission.Type == EDemoSquadMissionType::Control)));
		break;
	case EDemoSquadTreeTest::MissionType:
		Result = Mission.Type == Data.MissionType;
		break;
	case EDemoSquadTreeTest::MissionExpired:
		Result = Mission.Deadline > 0.0 && Squad->GetWorld()->GetTimeSeconds() >= Mission.Deadline;
		break;
	case EDemoSquadTreeTest::HasFallback:
		Result = Mission.bHasFallback;
		break;
	case EDemoSquadTreeTest::CommandMode:
		Result = Squad->GetCommandMode() == Data.CommandMode;
		break;
	case EDemoSquadTreeTest::ManualTactic:
		Result = Squad->GetManualTactic() == Data.Tactic;
		break;
	case EDemoSquadTreeTest::Engagement:
		Result = Mission.Engagement == Data.Engagement;
		break;
	case EDemoSquadTreeTest::PlanCurrent:
		Result = Squad->IsCurrentIdentity(Plan.Identity);
		break;
	case EDemoSquadTreeTest::Phase:
		Result = Plan.Phase == Data.Phase;
		break;
	case EDemoSquadTreeTest::RosterReady:
		Result = Squad->IsRosterReady();
		break;
	case EDemoSquadTreeTest::MetricAtLeast:
	{
		const auto Snapshot = Squad->GetSnapshot();
		float Value = 0.0f;
		switch (Data.Metric)
		{
		case EDemoSquadTreeMetric::EffectiveMembers:
			Value = Snapshot.EffectiveMemberCount;
			break;
		case EDemoSquadTreeMetric::MobileMembers:
			Value = Snapshot.MobileMemberCount;
			break;
		case EDemoSquadTreeMetric::ReadySupport:
			Value = Snapshot.ReadySupportCount;
			break;
		case EDemoSquadTreeMetric::AmmoReadiness:
			Value = Snapshot.AmmoReadiness;
			break;
		case EDemoSquadTreeMetric::ThreatPressure:
			Value = Squad->UsesMemberOrderPolicy() ? 0.0f : Snapshot.KnownThreatPressure;
			break;
		case EDemoSquadTreeMetric::SuppressionRatio:
			Value = Squad->UsesMemberOrderPolicy() ? 0.0f : Snapshot.SuppressedMemberRatio;
			break;
		case EDemoSquadTreeMetric::PhaseAge:
			Value = Squad->GetWorld()->GetTimeSeconds() - Plan.PhaseStartedAt;
			break;
		case EDemoSquadTreeMetric::PlanAge:
			Value = Squad->GetWorld()->GetTimeSeconds() - Plan.StartedAt;
			break;
		}
		Result = FMath::IsFinite(Data.Threshold) && Value >= Data.Threshold;
		break;
	}
	}
	return Result != Data.bInvert;
}
