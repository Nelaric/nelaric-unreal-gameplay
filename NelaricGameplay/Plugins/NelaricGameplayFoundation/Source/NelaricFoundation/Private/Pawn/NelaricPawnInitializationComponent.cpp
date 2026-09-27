// Copyright (c) 2026 Nelaric

#include "Pawn/NelaricPawnInitializationComponent.h"

UNelaricPawnInitializationComponent::UNelaricPawnInitializationComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

bool UNelaricPawnInitializationComponent::TryInitializePawn()
{
	if (bPawnInitialized)
	{
		return true;
	}

	if (!bInitializationAllowed || bInitializationInProgress || !GetPawn())
	{
		return false;
	}

	bInitializationInProgress = true;
	const bool bReady = CanInitializePawn();
	bInitializationInProgress = false;
	if (!bReady || !bInitializationAllowed || !GetPawn())
	{
		return false;
	}

	bPawnInitialized = true;
	OnPawnInitialized.Broadcast(this);
	return true;
}

bool UNelaricPawnInitializationComponent::IsPawnInitialized() const
{
	return bPawnInitialized;
}

bool UNelaricPawnInitializationComponent::CanInitializePawn_Implementation() const
{
	return GetPawn() != nullptr;
}

void UNelaricPawnInitializationComponent::BeginPlay()
{
	Super::BeginPlay();
	bInitializationAllowed = true;
	TryInitializePawn();
}

void UNelaricPawnInitializationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bInitializationAllowed = false;
	bPawnInitialized = false;
	Super::EndPlay(EndPlayReason);
}
