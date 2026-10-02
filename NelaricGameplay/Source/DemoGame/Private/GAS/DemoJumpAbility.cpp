// Copyright (c) 2026 Nelaric Contributors

#include "GAS/DemoJumpAbility.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoJumpAbility, Log, All);

UDemoJumpAbility::UDemoJumpAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}
void UDemoJumpAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                       FGameplayAbilityActivationInfo ActivationInfo,
                                       const FGameplayEventData* TriggerEventData)
{
	ACharacter* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get());
	if (!Character || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		if (!Character)
		{
			UE_LOG(LogDemoJumpAbility, Error, TEXT("Cannot activate jump ability %s: avatar is not a character."),
			       *GetName());
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	Character->Jump();
	auto* Task = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Task->OnRelease.AddDynamic(this, &ThisClass::OnReleased);
	Task->ReadyForActivation();
}
void UDemoJumpAbility::OnReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
void UDemoJumpAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                  FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
                                  bool bWasCancelled)
{
	if (ActorInfo)
	{
		if (auto* Character = Cast<ACharacter>(ActorInfo->AvatarActor.Get()))
		{
			Character->StopJumping();
		}
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
