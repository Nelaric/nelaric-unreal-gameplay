// Copyright (c) 2026 Nelaric Contributors

#include "Pawn/PawnControlComponent.h"

#include "AI/NelaricBotController.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Player/ControlSwitchSubsystem.h"

bool UPawnControlComponent::IsControlTransitionInProgress() const
{
	check(IsInGameThread());
	const UWorld* World = GetWorld();
	const UControlSwitchSubsystem* Coordinator = World ? World->GetSubsystem<UControlSwitchSubsystem>() : nullptr;
	return Coordinator && Coordinator->IsControlTransitionInProgress(GetPawn());
}

UPawnControlComponent::UPawnControlComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	ReturnControllerClass = ANelaricBotController::StaticClass();
}

bool UPawnControlComponent::CanEntryDataAvailable()
{
	APawn* Pawn = GetPawn();
	if (!Pawn)
	{
		return false;
	}
	AController* Controller = GetController();
	return !Controller || (IsValid(Controller->PlayerState) && !Controller->PlayerState->IsActorBeingDestroyed() &&
	                       Pawn->GetPlayerState() == Controller->PlayerState);
}

void UPawnControlComponent::OnInitReady()
{
	Super::OnInitReady();
	APawn* Pawn = GetPawn();
	AAIController* Controller = GetController<AAIController>();
	if (!Pawn || !Pawn->HasAuthority() || !Controller || !bStartBotLogicOnReady)
	{
		return;
	}
	for (AActor* Owner : {static_cast<AActor*>(Controller), static_cast<AActor*>(Pawn)})
	{
		if (!IsValid(Owner) || Owner->IsActorBeingDestroyed())
		{
			return;
		}
		TInlineComponentArray<UBrainComponent*> Brains(Owner);
		for (UBrainComponent* Brain : Brains)
		{
			if (GetController() != Controller || GetInitState() != Nelaric::EInitState::Ready)
			{
				return;
			}
			if (IsValid(Brain) && Brain->IsRegistered() && !Brain->IsRunning())
			{
				Brain->StartLogic();
			}
		}
	}
}

AAIController* UPawnControlComponent::FindReturnController() const
{
	const APawn* Pawn = GetPawn();
	const UWorld* World = GetWorld();
	if (!Pawn || !Pawn->HasAuthority() || !World || World->bIsTearingDown || !bReturnToBot)
	{
		return nullptr;
	}
	auto IsAvailable = [World](const AAIController* Controller)
	{
		return IsValid(Controller) && !Controller->IsActorBeingDestroyed() && Controller->GetWorld() == World &&
		       Controller->HasAuthority() && !Controller->GetPawn() && IsValid(Controller->PlayerState) &&
		       !Controller->PlayerState->IsActorBeingDestroyed() && Controller->PlayerState->GetWorld() == World &&
		       Controller->PlayerState->HasAuthority() && Controller->PlayerState->GetOwner() == Controller;
	};
	if (IsAvailable(RememberedController))
	{
		return RememberedController;
	}
	for (AAIController* Controller : SpawnedControllers)
	{
		if (IsAvailable(Controller))
		{
			return Controller;
		}
	}
	return nullptr;
}

void UPawnControlComponent::TrackSpawnedController(AAIController* Controller)
{
	SpawnedControllers.RemoveAll([](const AAIController* Existing)
	                             { return !IsValid(Existing) || Existing->IsActorBeingDestroyed(); });
	SpawnedControllers.AddUnique(Controller);
}

void UPawnControlComponent::ForgetSpawnedController(AAIController* Controller)
{
	SpawnedControllers.Remove(Controller);
}

void UPawnControlComponent::RememberController(AAIController* Controller)
{
	RememberedController = Controller;
}

void UPawnControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Destroy callbacks can reenter component teardown; detach ownership first.
	const auto OwnedControllers = MoveTemp(SpawnedControllers);
	RememberedController = nullptr;
	for (AAIController* Controller : OwnedControllers)
	{
		if (IsValid(Controller) && !Controller->IsActorBeingDestroyed() && !Controller->GetPawn())
		{
			const UWorld* World = GetWorld();
			const UControlSwitchSubsystem* Coordinator =
			    World ? World->GetSubsystem<UControlSwitchSubsystem>() : nullptr;
			if (!Coordinator || !Coordinator->IsControlTransitionInProgress(Controller))
			{
				Controller->Destroy();
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}
