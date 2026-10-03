// Copyright (c) 2026 Nelaric Contributors

#include "ObjectPool/CharacterPoolReplicationComponent.h"

#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "Templates/UnrealTemplate.h"

UCharacterPoolReplicationComponent::UCharacterPoolReplicationComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterPoolReplicationComponent::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Transition();
}

void UCharacterPoolReplicationComponent::EndPlay(EEndPlayReason::Type Reason)
{
	bEndingPlay = true;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeginPlayRetry);
	}
	Super::EndPlay(Reason);
}

void UCharacterPoolReplicationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UCharacterPoolReplicationComponent, Transition);
}

void UCharacterPoolReplicationComponent::Publish(bool bActive)
{
	check(IsInGameThread());
	check(GetOwner()->HasAuthority() && GetNetMode() != NM_Client);
	if (Transition && static_cast<bool>(Transition & 1) == bActive)
	{
		return;
	}
	check(Transition <= MAX_uint64 - 2);
	Transition = ((Transition & ~uint64{1}) + 2) | static_cast<uint64>(bActive);
	GetOwner()->ForceNetUpdate();
}

void UCharacterPoolReplicationComponent::OnRep_Transition()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(BeginPlayRetry);
	}
	ApplyTransition();
}

void UCharacterPoolReplicationComponent::ApplyTransition()
{
	check(IsInGameThread());
	UWorld* World = GetWorld();
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (bApplyingReplication || bEndingPlay || !World || World->bIsTearingDown || World->GetNetMode() != NM_Client ||
	    !IsValid(Character) || Character->IsActorBeingDestroyed() || !Transition || Transition == AppliedTransition)
	{
		return;
	}
	TGuardValue<bool> ApplyingGuard(bApplyingReplication, true);
	Nelaric::ObjectPool::FCharacterPoolHelper::DeactivateInternal(*Character, *State, true);
	if (!Character->HasActorBegunPlay())
	{
		if (HasBegunPlay() && !bQueuedBeginPlayRetry)
		{
			bQueuedBeginPlayRetry = true;
			BeginPlayRetry = World->GetTimerManager().SetTimerForNextTick(
			    FTimerDelegate::CreateUObject(this, &ThisClass::OnRep_Transition));
		}
		return;
	}
	World->GetTimerManager().ClearTimer(BeginPlayRetry);
	if (!(Transition & 1) || Nelaric::ObjectPool::FCharacterPoolHelper::ActivateInternal(
	                             *Character, *State, Character->GetActorTransform(), true))
	{
		AppliedTransition = Transition;
	}
}
