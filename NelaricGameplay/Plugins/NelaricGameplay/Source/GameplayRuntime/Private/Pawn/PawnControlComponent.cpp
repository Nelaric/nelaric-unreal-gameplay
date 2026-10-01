// Copyright (c) 2026 Nelaric Contributors

#include "Pawn/PawnControlComponent.h"

#include "AI/NelaricBotController.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

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

AAIController* UPawnControlComponent::PrepareReturnController()
{
	APawn* Pawn = GetPawn();
	UWorld* World = GetWorld();
	if (!Pawn || !Pawn->HasAuthority() || !World || World->bIsTearingDown || !bReturnToBot)
	{
		return nullptr;
	}
	if (IsValid(RememberedController) && !RememberedController->IsActorBeingDestroyed() &&
	    RememberedController->GetWorld() == World && !RememberedController->GetPawn() &&
	    IsValid(RememberedController->PlayerState) && !RememberedController->PlayerState->IsActorBeingDestroyed())
	{
		return RememberedController;
	}
	if (IsValid(SpawnedController) && !SpawnedController->GetPawn())
	{
		SpawnedController->Destroy();
	}
	SpawnedController = nullptr;
	if (!ReturnControllerClass || ReturnControllerClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return nullptr;
	}
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.OverrideLevel = Pawn->GetLevel();
	Parameters.ObjectFlags |= RF_Transient;
	AAIController* Controller = World->SpawnActor<AAIController>(ReturnControllerClass, Pawn->GetActorLocation(),
	                                                             Pawn->GetActorRotation(), Parameters);
	if (!IsValid(Controller) || Controller->IsActorBeingDestroyed() || Controller->GetPawn() ||
	    !IsValid(Controller->PlayerState) || Controller->PlayerState->IsActorBeingDestroyed())
	{
		if (IsValid(Controller) && !Controller->GetPawn())
		{
			Controller->Destroy();
		}
		return nullptr;
	}
	SpawnedController = Controller;
	RememberedController = Controller;
	return Controller;
}

void UPawnControlComponent::RememberController(AAIController* Controller)
{
	RememberedController = Controller;
}

void UPawnControlComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(SpawnedController) && !SpawnedController->GetPawn())
	{
		SpawnedController->Destroy();
	}
	RememberedController = nullptr;
	SpawnedController = nullptr;
	Super::EndPlay(EndPlayReason);
}
