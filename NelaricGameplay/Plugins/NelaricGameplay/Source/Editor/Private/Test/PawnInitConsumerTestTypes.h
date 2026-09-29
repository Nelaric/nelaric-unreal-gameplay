// Copyright (c) 2026 Nelaric

#pragma once

#include "Pawn/NelaricPawn.h"
#include "Pawn/PawnInitStateComponent.h"
#include "Pawn/PawnInitializationComponent.h"

#include "PawnInitConsumerTestTypes.generated.h"

UCLASS(MinimalAPI)
class UInitConsumerManager final : public UPawnInitializationComponent
{
	GENERATED_BODY()

public:
	bool bContextAvailable = false;

protected:
	virtual bool CanInitializePawn_Implementation() const override
	{
		return bContextAvailable && Super::CanInitializePawn_Implementation();
	}
};

UCLASS(MinimalAPI)
class AInitConsumerPawn final : public ANelaricPawn
{
	GENERATED_BODY()

public:
	AInitConsumerPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get())
	    : Super(ObjectInitializer.SetDefaultSubobjectClass<UInitConsumerManager>(TEXT("PawnInitializationComponent")))
	{
	}
};

UCLASS(MinimalAPI)
class UInitConsumerComponent final : public UPawnInitStateComponent
{
	GENERATED_BODY()

public:
	bool bDataAvailable = false;
	int32 ReadyCalls = 0;
	int32 CleanupCalls = 0;

protected:
	virtual bool CanEntryDataAvailable() override
	{
		return bDataAvailable && Super::CanEntryDataAvailable();
	}
	virtual bool CanEntryDataInitialized() override
	{
		return Super::CanEntryDataInitialized();
	}
	virtual bool CanEntryReady() override
	{
		return Super::CanEntryReady();
	}
	virtual void OnInitReady() override
	{
		Super::OnInitReady();
		++ReadyCalls;
	}
	virtual void OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot& Previous) override
	{
		Super::OnInitGenerationInvalidated(Previous);
		++CleanupCalls;
	}
};

UCLASS(MinimalAPI)
class UInitConsumerObserver final : public UObject
{
	GENERATED_BODY()

public:
	int32 ReadyCalls = 0;
	int32 RevokedCalls = 0;

	UFUNCTION()
	void OnReady(UPawnInitializationComponent* Manager)
	{
		++ReadyCalls;
	}
	UFUNCTION()
	void OnRevoked(UPawnInitializationComponent* Manager)
	{
		++RevokedCalls;
	}
};
