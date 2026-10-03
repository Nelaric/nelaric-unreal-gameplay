// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "Components/ActorComponent.h"
#include "ObjectPool/CharacterPoolHelper.h"
#include "TimerManager.h"

#include "CharacterPoolReplicationComponent.generated.h"

namespace Nelaric::ObjectPool
{
struct FCharacterPoolHelper;
}

// Private network adapter; the native pool and interface remain unreflected.
UCLASS(MinimalAPI)
class UCharacterPoolReplicationComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterPoolReplicationComponent(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	friend struct Nelaric::ObjectPool::FCharacterPoolHelper;

	// Low bit is active; upper bits distinguish collapsed return/acquire pairs.
	UPROPERTY(ReplicatedUsing = OnRep_Transition)
	uint64 Transition = 0;
	uint64 AppliedTransition = 0;
	bool bApplyingReplication = false;
	bool bQueuedBeginPlayRetry = false;
	bool bEndingPlay = false;
	Nelaric::ObjectPool::FCharacterPoolState UnboundState;
	Nelaric::ObjectPool::FCharacterPoolState* State = &UnboundState;
	FTimerHandle BeginPlayRetry;

	UFUNCTION()
	void OnRep_Transition();
	void ApplyTransition();
	void Publish(bool bActive);
};
