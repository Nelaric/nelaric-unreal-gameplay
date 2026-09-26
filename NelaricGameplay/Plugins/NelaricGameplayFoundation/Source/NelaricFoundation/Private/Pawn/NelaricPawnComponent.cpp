// Copyright (c) 2026 Nelaric

#include "Pawn/NelaricPawnComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

UNelaricPawnComponent::UNelaricPawnComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

APawn* UNelaricPawnComponent::GetPawn() const
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	return IsValid(Pawn) && !Pawn->IsActorBeingDestroyed() ? Pawn : nullptr;
}

AController* UNelaricPawnComponent::GetController() const
{
	const APawn* Pawn = GetPawn();
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	return IsValid(Controller) && !Controller->IsActorBeingDestroyed() ? Controller : nullptr;
}

APlayerController* UNelaricPawnComponent::GetPlayerController() const
{
	return Cast<APlayerController>(GetController());
}

APlayerState* UNelaricPawnComponent::GetPlayerState() const
{
	const APawn* Pawn = GetPawn();
	APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	return IsValid(PlayerState) && !PlayerState->IsActorBeingDestroyed() ? PlayerState : nullptr;
}

bool UNelaricPawnComponent::IsLocallyControlled() const
{
	const APawn* Pawn = GetPawn();
	return Pawn && Pawn->IsLocallyControlled();
}

bool UNelaricPawnComponent::IsPlayerControlled() const
{
	const APawn* Pawn = GetPawn();
	return Pawn && Pawn->IsPlayerControlled();
}

void UNelaricPawnComponent::OnRegister()
{
	Super::OnRegister();
	if (!IsTemplate())
	{
		ensureMsgf(Cast<APawn>(GetOwner()), TEXT("NelaricPawnComponent requires a Pawn owner: %s"), *GetPathName());
	}
}
