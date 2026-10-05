// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSoldierStateTreeNodes.h"
#include "AI/DemoSoldierComponent.h"
#include "AIController.h"
#include "Components/StateTreeComponent.h"
#include "StateTreeExecutionContext.h"

const UStruct* FDemoSoldierEvaluator::GetInstanceDataType() const
{
	return FDemoSoldierEvaluatorData::StaticStruct();
}

namespace Nelaric::Soldier
{
static void CopySnapshot(FDemoSoldierEvaluatorData& Data, const UDemoSoldierComponent* Soldier)
{
	Data.Memory = Soldier ? Soldier->GetMemory() : FDemoSoldierMemory();
	Data.Order = Soldier ? Soldier->GetOrder() : FDemoSoldierOrder();
	Data.OrderStatus = Soldier ? Soldier->GetOrderStatus() : EDemoSoldierOrderStatus::None;
	Data.Behavior = Soldier ? Soldier->GetBehavior() : EDemoSoldierBehavior::Idle;
}
} // namespace Nelaric::Soldier

void FDemoSoldierEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	auto& Data = Context.GetInstanceData<FDemoSoldierEvaluatorData>(*this);
	UDemoSoldierComponent* Soldier = Data.Pawn ? Data.Pawn->FindComponentByClass<UDemoSoldierComponent>() : nullptr;
	Nelaric::Soldier::CopySnapshot(Data, Soldier);
	Data.Soldier = Soldier;
	if (Soldier)
	{
		Data.EventHandle = Soldier->OnEvent().AddLambda(
		    [WeakContext = Context.MakeWeakExecutionContext(), WeakSoldier = Data.Soldier](FGameplayTag)
		    {
			    auto Strong = WeakContext.MakeStrongExecutionContext();
			    if (auto* Live = Strong.GetInstanceDataPtr<FDemoSoldierEvaluatorData>())
			    {
				    Nelaric::Soldier::CopySnapshot(*Live, WeakSoldier.Get());
			    }
		    });
	}
}

void FDemoSoldierEvaluator::TreeStop(FStateTreeExecutionContext& Context) const
{
	auto& Data = Context.GetInstanceData<FDemoSoldierEvaluatorData>(*this);
	if (UDemoSoldierComponent* Soldier = Data.Soldier.Get())
	{
		Soldier->OnEvent().Remove(Data.EventHandle);
	}
	Data.EventHandle.Reset();
	Data.Soldier.Reset();
}

FDemoSoldierRunTask::FDemoSoldierRunTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = false;
}

const UStruct* FDemoSoldierRunTask::GetInstanceDataType() const
{
	return FDemoSoldierRunTaskData::StaticStruct();
}

EStateTreeRunStatus FDemoSoldierRunTask::EnterState(FStateTreeExecutionContext& Context,
                                                    const FStateTreeTransitionResult& Transition) const
{
	auto& Data = Context.GetInstanceData<FDemoSoldierRunTaskData>(*this);
	AAIController* Bot = Cast<AAIController>(Data.Controller);
	UDemoSoldierComponent* Soldier = Data.Pawn ? Data.Pawn->FindComponentByClass<UDemoSoldierComponent>() : nullptr;
	if (!Soldier || !Data.StateTreeComponent || Soldier->IsExecutingFor(Data.StateTreeComponent) ||
	    !Soldier->StartExecution(Bot, Data.StateTreeComponent, Data.bRunNativePlanner))
	{
		UE_LOG(LogStateTree, Warning,
		       TEXT("Run Demo Soldier failed: pawn=%s controller=%s soldier=%s treeComponent=%s. Check full Pawn "
		            "Ready, GAS, and a single global Run Demo Soldier task."),
		       *GetNameSafe(Data.Pawn), *GetNameSafe(Bot), *GetNameSafe(Soldier),
		       *GetNameSafe(Data.StateTreeComponent));
		return EStateTreeRunStatus::Failed;
	}
	Data.Soldier = Soldier;
	if (!Data.bRunNativePlanner)
	{
		Soldier->EnsureInitialTreeOrder(Data.InitialOrder, Data.InitialHoldRadius);
	}
	return EStateTreeRunStatus::Running;
}

void FDemoSoldierRunTask::ExitState(FStateTreeExecutionContext& Context,
                                    const FStateTreeTransitionResult& Transition) const
{
	auto& Data = Context.GetInstanceData<FDemoSoldierRunTaskData>(*this);
	if (UDemoSoldierComponent* Soldier = Data.Soldier.Get())
	{
		Soldier->StopExecution(Data.StateTreeComponent);
	}
	Data.Soldier.Reset();
}

namespace Nelaric::Soldier
{
static EStateTreeRunStatus TreeRunStatus(EDemoSoldierTreeResult Result)
{
	switch (Result)
	{
	case EDemoSoldierTreeResult::Running:
		return EStateTreeRunStatus::Running;
	case EDemoSoldierTreeResult::Succeeded:
		return EStateTreeRunStatus::Succeeded;
	default:
		return EStateTreeRunStatus::Failed;
	}
}
} // namespace Nelaric::Soldier

FDemoSoldierActionTask::FDemoSoldierActionTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = true;
}

const UStruct* FDemoSoldierActionTask::GetInstanceDataType() const
{
	return FDemoSoldierActionTaskData::StaticStruct();
}

EStateTreeRunStatus FDemoSoldierActionTask::EnterState(FStateTreeExecutionContext& Context,
                                                       const FStateTreeTransitionResult& Transition) const
{
	auto& Data = Context.GetInstanceData<FDemoSoldierActionTaskData>(*this);
	UDemoSoldierComponent* Soldier = Data.Pawn ? Data.Pawn->FindComponentByClass<UDemoSoldierComponent>() : nullptr;
	if (!Soldier || !Soldier->BeginTreeAction(Data.Action, Data.StateTreeComponent, Data.RequestId))
	{
		UE_LOG(LogStateTree, Warning,
		       TEXT("Demo Soldier Action failed to start: pawn=%s action=%d treeComponent=%s. Configure one global Run "
		            "Demo Soldier with Run Native Planner disabled."),
		       *GetNameSafe(Data.Pawn), static_cast<int32>(Data.Action), *GetNameSafe(Data.StateTreeComponent));
		Data.Soldier.Reset();
		return EStateTreeRunStatus::Failed;
	}
	Data.Soldier = Soldier;
	Data.FinishedHandle = Soldier->OnTreeActionFinished().AddLambda(
	    [WeakContext = Context.MakeWeakExecutionContext(), Request = Data.RequestId](FGuid Id,
	                                                                                 EDemoSoldierTreeResult Result)
	    {
		    if (Id == Request)
		    {
			    WeakContext.FinishTask(Result == EDemoSoldierTreeResult::Succeeded ? EStateTreeFinishTaskType::Succeeded
			                                                                       : EStateTreeFinishTaskType::Failed);
		    }
	    });
	return Nelaric::Soldier::TreeRunStatus(Soldier->GetTreeActionResult(Data.RequestId));
}

void FDemoSoldierActionTask::ExitState(FStateTreeExecutionContext& Context,
                                       const FStateTreeTransitionResult& Transition) const
{
	auto& Data = Context.GetInstanceData<FDemoSoldierActionTaskData>(*this);
	if (UDemoSoldierComponent* Soldier = Data.Soldier.Get())
	{
		Soldier->OnTreeActionFinished().Remove(Data.FinishedHandle);
		Soldier->EndTreeAction(Data.RequestId);
	}
	Data.FinishedHandle.Reset();
	Data.RequestId.Invalidate();
	Data.Soldier.Reset();
}

const UStruct* FDemoSoldierCondition::GetInstanceDataType() const
{
	return FDemoSoldierConditionData::StaticStruct();
}

bool FDemoSoldierCondition::TestCondition(FStateTreeExecutionContext& Context) const
{
	const auto& Data = Context.GetInstanceData<FDemoSoldierConditionData>(*this);
	const UDemoSoldierComponent* Soldier =
	    Data.Pawn ? Data.Pawn->FindComponentByClass<UDemoSoldierComponent>() : nullptr;
	return Soldier && (Soldier->TestTreeCondition(Data.Test) != Data.bInvert);
}
