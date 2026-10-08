// Copyright (c) 2026 Nelaric Contributors

#include "GAS/DemoWeaponAbilities.h"

#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Character/DemoCharacter.h"
#include "Engine/World.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "Equipment/DemoEquipmentManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "GAS/DemoWeaponTags.h"

namespace Nelaric::DemoEquipment
{
static UDemoWeaponInstance* ActiveWeapon(const FGameplayAbilityActorInfo* Info)
{
	const APawn* Pawn = Info ? Cast<APawn>(Info->AvatarActor.Get()) : nullptr;
	const UDemoEquipmentManagerComponent* Manager =
	    Pawn ? Pawn->FindComponentByClass<UDemoEquipmentManagerComponent>() : nullptr;
	return Manager ? Manager->GetActiveWeapon() : nullptr;
}
} // namespace Nelaric::DemoEquipment

UDemoFireAbility::UDemoFireAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	SetAssetTags(FGameplayTagContainer(Nelaric::DemoWeaponTags::FireAbility));
}

bool UDemoFireAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                          const FGameplayTagContainer* SourceTags,
                                          const FGameplayTagContainer* TargetTags,
                                          FGameplayTagContainer* OptionalRelevantTags) const
{
	const UDemoWeaponInstance* Item = Nelaric::DemoEquipment::ActiveWeapon(ActorInfo);
	const ADemoCharacter* Character = ActorInfo ? Cast<ADemoCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return Item && Item->GetWeaponState().MagazineAmmo > 0 && !Item->GetWeaponState().bReloading &&
	       (!Character || (Character->IsAlive() && Character->IsPoolActive() && !Character->IsBattlefrontFrozen())) &&
	       Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UDemoFireAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                       FGameplayAbilityActivationInfo ActivationInfo,
                                       const FGameplayEventData* TriggerEventData)
{
	Weapon = Nelaric::DemoEquipment::ActiveWeapon(ActorInfo);
	if (!Weapon.IsValid() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	CanceledHandle = Weapon->OnActionsCanceled().AddUObject(this, &ThisClass::HandleWeaponCanceled);
	UAbilityTask_WaitInputRelease* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Release->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	Release->ReadyForActivation();
	if (IsActive() && ActorInfo->IsNetAuthority())
	{
		FireNextShot();
	}
}

void UDemoFireAbility::FireNextShot()
{
	if (!IsActive())
	{
		return;
	}
	UDemoWeaponInstance* Item = Weapon.Get();
	if (!Item || Nelaric::DemoEquipment::ActiveWeapon(CurrentActorInfo) != Item ||
	    Item->TryFire() != EDemoWeaponResult::Success)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}
	if (!IsActive())
	{
		return;
	}
	const UDemoWeaponDefinition* Definition = Item->GetWeaponDefinition();
	if (!Definition || !Definition->bAutomatic)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &ThisClass::FireNextShot, Definition->FireInterval, false);
}

void UDemoFireAbility::HandleInputReleased(float TimeHeld)
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UDemoFireAbility::HandleWeaponCanceled()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UDemoFireAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                  FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
                                  bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimer);
	}
	if (UDemoWeaponInstance* Item = Weapon.Get())
	{
		Item->OnActionsCanceled().Remove(CanceledHandle);
	}
	CanceledHandle.Reset();
	Weapon.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

UDemoReloadAbility::UDemoReloadAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	SetAssetTags(FGameplayTagContainer(Nelaric::DemoWeaponTags::ReloadAbility));
	CancelAbilitiesWithTag.AddTag(Nelaric::DemoWeaponTags::FireAbility);
	BlockAbilitiesWithTag.AddTag(Nelaric::DemoWeaponTags::FireAbility);
}

bool UDemoReloadAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle,
                                            const FGameplayAbilityActorInfo* ActorInfo,
                                            const FGameplayTagContainer* SourceTags,
                                            const FGameplayTagContainer* TargetTags,
                                            FGameplayTagContainer* OptionalRelevantTags) const
{
	const UDemoWeaponInstance* Item = Nelaric::DemoEquipment::ActiveWeapon(ActorInfo);
	const UDemoWeaponDefinition* Definition = Item ? Item->GetWeaponDefinition() : nullptr;
	const ADemoCharacter* Character = ActorInfo ? Cast<ADemoCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	return Definition && !Item->GetWeaponState().bReloading && Item->GetWeaponState().ReserveAmmo > 0 &&
	       Item->GetWeaponState().MagazineAmmo < Definition->MagazineCapacity &&
	       (!Character || (Character->IsAlive() && Character->IsPoolActive() && !Character->IsBattlefrontFrozen())) &&
	       Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UDemoReloadAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                         FGameplayAbilityActivationInfo ActivationInfo,
                                         const FGameplayEventData* TriggerEventData)
{
	Weapon = Nelaric::DemoEquipment::ActiveWeapon(ActorInfo);
	if (!Weapon.IsValid() || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	CanceledHandle = Weapon->OnActionsCanceled().AddUObject(this, &ThisClass::HandleWeaponCanceled);
	if (ActorInfo->IsNetAuthority())
	{
		FinishedHandle = Weapon->OnReloadFinished().AddUObject(this, &ThisClass::HandleReloadFinished);
		if (Weapon->BeginReload(ReloadId) != EDemoWeaponResult::Success)
		{
			EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		}
	}
}

void UDemoReloadAbility::HandleReloadFinished(FGuid FinishedId, EDemoWeaponResult Result)
{
	if (IsActive() && ReloadId == FinishedId)
	{
		ReloadId.Invalidate();
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true,
		           Result != EDemoWeaponResult::Success);
	}
}

void UDemoReloadAbility::HandleWeaponCanceled()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
	}
}

void UDemoReloadAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
                                    FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
                                    bool bWasCancelled)
{
	if (UDemoWeaponInstance* Item = Weapon.Get())
	{
		Item->OnActionsCanceled().Remove(CanceledHandle);
		Item->OnReloadFinished().Remove(FinishedHandle);
		if (ActorInfo && ActorInfo->IsNetAuthority() && ReloadId.IsValid())
		{
			Item->CancelReload(ReloadId);
		}
	}
	CanceledHandle.Reset();
	FinishedHandle.Reset();
	ReloadId.Invalidate();
	Weapon.Reset();
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
