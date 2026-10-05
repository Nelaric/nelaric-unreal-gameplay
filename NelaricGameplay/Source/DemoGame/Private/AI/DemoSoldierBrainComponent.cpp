// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSoldierBrainComponent.h"
#include "AI/DemoSoldierComponent.h"
#include "AI/DemoSoldierController.h"
#include "AIController.h"

UDemoSoldierBrainComponent::UDemoSoldierBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDemoSoldierBrainComponent::StartLogic()
{
	AAIController* Bot = Cast<AAIController>(GetOwner());
	if (const ADemoSoldierController* SoldierController = Cast<ADemoSoldierController>(Bot);
	    SoldierController && !SoldierController->bUseNativeBrain)
	{
		return;
	}
	UDemoSoldierComponent* Component =
	    Bot && Bot->GetPawn() ? Bot->GetPawn()->FindComponentByClass<UDemoSoldierComponent>() : nullptr;
	if (!Component || bPaused || IsResourceLocked() || !Component->StartExecution(Bot, this))
	{
		return;
	}
	Soldier = Component;
}

void UDemoSoldierBrainComponent::RestartLogic()
{
	StopLogic(TEXT("Soldier execution restarted"));
	StartLogic();
}

void UDemoSoldierBrainComponent::StopLogic(const FString& Reason)
{
	if (Soldier.IsValid())
	{
		Soldier->StopExecution(this);
	}
	Soldier.Reset();
	bPaused = false;
}

void UDemoSoldierBrainComponent::Cleanup()
{
	StopLogic(TEXT("Soldier brain cleanup"));
}

void UDemoSoldierBrainComponent::PauseLogic(const FString& Reason)
{
	if (IsRunning())
	{
		StopLogic(Reason);
		bPaused = true;
	}
}

EAILogicResuming::Type UDemoSoldierBrainComponent::ResumeLogic(const FString& Reason)
{
	const EAILogicResuming::Type Result = Super::ResumeLogic(Reason);
	if (Result == EAILogicResuming::Continue)
	{
		bPaused = false;
		StartLogic();
	}
	return Result;
}

bool UDemoSoldierBrainComponent::IsRunning() const
{
	return bPaused || (Soldier.IsValid() && Soldier->IsExecutingFor(this));
}

bool UDemoSoldierBrainComponent::IsPaused() const
{
	return bPaused;
}

FString UDemoSoldierBrainComponent::GetDebugInfoString() const
{
	return Soldier.IsValid() ? FString::Printf(TEXT("Soldier action=%d order=%d"), int32(Soldier->GetBehavior()),
	                                           int32(Soldier->GetOrderStatus()))
	                         : TEXT("Soldier stopped");
}

void UDemoSoldierBrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Cleanup();
	Super::EndPlay(EndPlayReason);
}
