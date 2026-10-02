// Copyright (c) 2026 Nelaric Contributors

#include "NelaricGameplayAbility.h"
#include "NelaricAbilitySystemComponent.h"
#include "PawnGasBindingComponent.h"
#include "GameFramework/Pawn.h"

UNelaricGameplayAbility::UNelaricGameplayAbility(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}
bool UNelaricGameplayAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle,
                                                 const FGameplayAbilityActorInfo* ActorInfo,
                                                 const FGameplayTagContainer* SourceTags,
                                                 const FGameplayTagContainer* TargetTags,
                                                 FGameplayTagContainer* OptionalRelevantTags) const
{
	const auto* ASC =
	    ActorInfo ? Cast<UNelaricAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	const auto* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const auto* Binding = Pawn ? Pawn->FindComponentByClass<UPawnGasBindingComponent>() : nullptr;
	return ASC && Binding && Binding->IsReadyForActions() && !ASC->IsTransferringState() &&
	       Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UNelaricGameplayAbility::ApplyCooldown(FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo,
                                            FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* Effect = GetCooldownGameplayEffect();
	if (!Effect)
	{
		return;
	}
	const auto Active =
	    ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, Effect, GetAbilityLevel(Handle, ActorInfo));
	auto* ASC = ActorInfo ? Cast<UNelaricAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()) : nullptr;
	auto* Pawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (ASC && Pawn && Pawn->HasAuthority() && CooldownOwnership != EGasStateOwnership::Participant)
	{
		ASC->TrackEffect(Active, Pawn, CooldownOwnership);
	}
}
