// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "Pawn/PawnInitStateComponent.h"
#include "Pawn/PawnInitializationComponent.h"
#include "Templates/Function.h"

#include "PawnInitRegressionTestTypes.generated.h"

UCLASS(MinimalAPI)
class UInitRegressionComponent final : public UPawnInitStateComponent
{
	GENERATED_BODY()

public:
	bool bApplicable = true;
	bool bAllowReady = false;
	bool bGameplayActive = false;
	int32 PrepareCalls = 0;
	int32 ReadyCalls = 0;
	int32 CleanupCalls = 0;
	TArray<Nelaric::FInitGeneration> ReadyGenerations;
	TFunction<void()> PrepareAction;
	TFunction<void()> ReadyAction;
	TFunction<void()> CleanupAction;

	virtual bool IsInitApplicable() const override
	{
		return bApplicable;
	}

protected:
	virtual bool CanEntryReady() override
	{
		++PrepareCalls;
		if (!bAllowReady)
		{
			return false;
		}
		TFunction<void()> Action = MoveTemp(PrepareAction);
		if (Action)
		{
			Action();
		}
		return bAllowReady;
	}

	virtual void OnInitReady() override
	{
		++ReadyCalls;
		ReadyGenerations.Add(GetInitGeneration());
		bGameplayActive = true;
		TFunction<void()> Action = MoveTemp(ReadyAction);
		if (Action)
		{
			Action();
		}
	}

	virtual void OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot&) override
	{
		++CleanupCalls;
		bGameplayActive = false;
		if (CleanupAction)
		{
			CleanupAction();
		}
	}
};

UCLASS(MinimalAPI)
class UInitRegressionObserver final : public UObject
{
	GENERATED_BODY()

public:
	int32 ReadyCalls = 0;
	int32 RevokedCalls = 0;
	bool bGameplayActive = false;
	bool bInvalidReadyNotification = false;
	TArray<FName> Events;

	UFUNCTION()
	void OnReady(UPawnInitializationComponent* Manager)
	{
		++ReadyCalls;
		bGameplayActive = true;
		bInvalidReadyNotification |= !Manager->IsPawnInitialized();
		Events.Add(TEXT("Ready"));
	}

	UFUNCTION()
	void OnRevoked(UPawnInitializationComponent* Manager)
	{
		++RevokedCalls;
		bGameplayActive = false;
		Events.Add(TEXT("Revoked"));
	}
};
