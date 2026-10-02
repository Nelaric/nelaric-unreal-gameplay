// Copyright (c) 2026 Nelaric Contributors

/** @file DemoJumpAbility.h Declares shared player and bot jump execution. */
#pragma once
#include "NelaricGameplayAbility.h"
#include "DemoJumpAbility.generated.h"

/// Jumps on action press and stops on release or control cancellation.
UCLASS(MinimalAPI)
class UDemoJumpAbility : public UNelaricGameplayAbility
{
	GENERATED_BODY()
public:
public:
	UDemoJumpAbility();
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                        FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
	                        bool bWasCancelled) override;

private:
	UFUNCTION()
	void OnReleased(float TimeHeld);
};
