// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "Pawn/NelaricPawn.h"
#include "Pawn/PawnInitStateComponent.h"

#include "PawnInitNetworkEventTestTypes.generated.h"

UCLASS(MinimalAPI)
class UInitNetworkEventComponent final : public UPawnInitStateComponent
{
	GENERATED_BODY()

public:
	int32 ReadyCalls = 0;
	int32 CleanupCalls = 0;
	TWeakObjectPtr<AController> ReadyController;
	TWeakObjectPtr<APlayerState> ReadyPlayerState;
	bool bHadLocalInput = false;

protected:
	virtual bool CanEntryReady() override;
	virtual void OnInitReady() override;
	virtual void OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot& Previous) override;
};

UCLASS(MinimalAPI)
class AInitNetworkEventPawn final : public ANelaricPawn
{
	GENERATED_BODY()

public:
	AInitNetworkEventPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated)
	int32 TestPawnId = 0;
};
