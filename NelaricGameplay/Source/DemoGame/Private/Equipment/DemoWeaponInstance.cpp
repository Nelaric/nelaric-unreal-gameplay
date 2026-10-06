// Copyright (c) 2026 Nelaric Contributors

#include "Equipment/DemoEquipmentInstance.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/DemoCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoEquipmentManagerComponent.h"
#include "Equipment/DemoPawnAnimationLayerComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GAS/DemoCombatAttributes.h"
#include "GAS/DemoWeaponTags.h"
#include "Kismet/GameplayStatics.h"
#include "NelaricAbilitySystemComponent.h"
#include "PawnGasBindingComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Templates/UnrealTemplate.h"

namespace Nelaric::DemoEquipment
{
static float ServerTime(const UWorld& World)
{
	const AGameStateBase* State = World.GetGameState();
	return State ? State->GetServerWorldTimeSeconds() : World.GetTimeSeconds();
}

static USkeletalMeshComponent* PresentationMesh(APawn* OwnerPawn)
{
	if (!OwnerPawn)
	{
		return nullptr;
	}
	const UDemoPawnAnimationLayerComponent* Provider =
	    OwnerPawn->FindComponentByClass<UDemoPawnAnimationLayerComponent>();
	const FName Name = Provider ? Provider->MeshComponentName : FName(TEXT("CharacterMesh0"));
	TInlineComponentArray<USkeletalMeshComponent*> Meshes(OwnerPawn);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		if (IsValid(Mesh) && Mesh->GetFName() == Name)
		{
			return Mesh;
		}
	}
	return nullptr;
}
} // namespace Nelaric::DemoEquipment

FDemoWeaponState UDemoWeaponInstance::GetWeaponState() const
{
	check(IsInGameThread());
	return WeaponState;
}

float UDemoWeaponInstance::GetTimeSinceFiredWeapon() const
{
	check(IsInGameThread());
	const UWorld* World = GetWorld();
	if (!World || WeaponState.ShotsFired <= 0 || !FMath::IsFinite(WeaponState.LastFireServerTime))
	{
		return -1.0f;
	}
	const float Now = Nelaric::DemoEquipment::ServerTime(*World);
	return FMath::IsFinite(Now) ? FMath::Max(0.0f, Now - WeaponState.LastFireServerTime) : -1.0f;
}

float UDemoWeaponInstance::GetReloadRemainingTime() const
{
	check(IsInGameThread());
	const UWorld* World = GetWorld();
	return WeaponState.bReloading && World
	           ? FMath::Max(0.0f, WeaponState.ReloadEndServerTime - Nelaric::DemoEquipment::ServerTime(*World))
	           : 0.0f;
}

UDemoEquipmentManagerComponent* UDemoWeaponInstance::GetManager() const
{
	return Cast<UDemoEquipmentManagerComponent>(GetOuter());
}

void UDemoWeaponInstance::InitializeWeaponState()
{
	const UDemoWeaponDefinition* WeaponDefinition = GetWeaponDefinition();
	WeaponState = {};
	if (WeaponDefinition)
	{
		WeaponState.MagazineAmmo = WeaponDefinition->MagazineCapacity;
		WeaponState.ReserveAmmo = WeaponDefinition->InitialReserveAmmo;
	}
}

EDemoWeaponResult UDemoWeaponInstance::CheckWeaponCommand() const
{
	check(IsInGameThread());
	const APawn* OwnerPawn = GetPawn();
	const UWorld* World = GetWorld();
	const UDemoEquipmentManagerComponent* Manager = GetManager();
	if (!OwnerPawn || !World || World->bIsTearingDown || !Manager || Manager->bEnding || !Manager->bStarted ||
	    Manager->GetInitState() != Nelaric::EInitState::Ready)
	{
		return EDemoWeaponResult::NotReady;
	}
	if (!OwnerPawn->HasAuthority())
	{
		return EDemoWeaponResult::NotAuthority;
	}
	if (bWeaponMutating || bCancelingActions || Manager->bMutating)
	{
		return EDemoWeaponResult::Busy;
	}
	if (!IsActive() || Manager->GetActiveWeapon() != this)
	{
		return EDemoWeaponResult::Inactive;
	}
	const UDemoWeaponDefinition* WeaponDefinition = GetWeaponDefinition();
	if (!WeaponDefinition || !WeaponDefinition->IsCombatConfigurationValid())
	{
		return EDemoWeaponResult::InvalidDefinition;
	}
	const ADemoCharacter* Character = Cast<ADemoCharacter>(OwnerPawn);
	if (Character && (!Character->IsPoolActive() || !Character->IsAlive()))
	{
		return EDemoWeaponResult::Unavailable;
	}
	const UPawnGasBindingComponent* Binding = OwnerPawn->FindComponentByClass<UPawnGasBindingComponent>();
	return OwnerPawn->GetController() && Binding && Binding->IsReadyForActions() ? EDemoWeaponResult::Success
	                                                                             : EDemoWeaponResult::NotReady;
}

EDemoWeaponResult UDemoWeaponInstance::TryFire()
{
	const EDemoWeaponResult Result = CheckWeaponCommand();
	if (Result != EDemoWeaponResult::Success)
	{
		return Result;
	}
	if (WeaponState.bReloading)
	{
		return EDemoWeaponResult::Reloading;
	}
	if (WeaponState.MagazineAmmo <= 0)
	{
		return EDemoWeaponResult::EmptyMagazine;
	}
	UWorld* World = GetWorld();
	if (World->GetTimeSeconds() + UE_SMALL_NUMBER < NextAllowedFireTime)
	{
		return EDemoWeaponResult::RateLimited;
	}
	APawn* OwnerPawn = GetPawn();
	UDemoWeaponDefinition* WeaponDefinition = GetWeaponDefinition();
	const FVector Start = OwnerPawn->GetPawnViewLocation();
	const FRotator Rotation = OwnerPawn->GetBaseAimRotation();
	if (Start.ContainsNaN() || Rotation.ContainsNaN())
	{
		return EDemoWeaponResult::NotReady;
	}
	TGuardValue<bool> Guard(bWeaponMutating, true);
	FDemoWeaponShot Shot;
	Shot.EquipmentId = GetEquipmentId();
	Shot.Definition = WeaponDefinition;
	Shot.TraceStart = Start;
	const FVector End = Start + Rotation.Vector() * WeaponDefinition->Range;
	FCollisionQueryParams Query(SCENE_QUERY_STAT(DemoWeaponShot), false, OwnerPawn);
	Query.bReturnPhysicalMaterial = true;
	World->LineTraceSingleByChannel(Shot.Hit, Start, End, WeaponDefinition->TraceChannel, Query);
	Shot.TraceEnd = Shot.Hit.bBlockingHit ? Shot.Hit.ImpactPoint : End;
	const FDemoWeaponState Previous = WeaponState;
	--WeaponState.MagazineAmmo;
	if (WeaponState.ShotsFired < MAX_int32)
	{
		++WeaponState.ShotsFired;
	}
	WeaponState.LastFireServerTime = Nelaric::DemoEquipment::ServerTime(*World);
	NextAllowedFireTime = World->GetTimeSeconds() + WeaponDefinition->FireInterval;
	UAISense_Hearing::ReportNoiseEvent(World, Start, 1.0f, OwnerPawn, WeaponDefinition->Range, TEXT("Gunshot"));

	ADemoCharacter* Target = Cast<ADemoCharacter>(Shot.Hit.GetActor());
	const UPawnGasBindingComponent* TargetBinding = Target ? Target->GetGasBinding() : nullptr;
	UNelaricAbilitySystemComponent* SourceASC =
	    OwnerPawn->FindComponentByClass<UPawnGasBindingComponent>()->GetAbilitySystem();
	if (Target && Target->CanBeDamaged() && Target->IsPoolActive() && Target->IsAlive() && TargetBinding &&
	    TargetBinding->IsReadyForActions() && WeaponDefinition->DamagePerShot > 0.0f)
	{
		UNelaricAbilitySystemComponent* TargetASC = TargetBinding->GetAbilitySystem();
		FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
		Context.AddSourceObject(WeaponDefinition);
		Context.AddInstigator(OwnerPawn, OwnerPawn);
		Context.AddHitResult(Shot.Hit, true);
		FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(WeaponDefinition->DamageEffect, 1.0f, Context);
		if (Spec.IsValid())
		{
			const float PreviousHealth = Target->GetHealth();
			Spec.Data->SetSetByCallerMagnitude(Nelaric::DemoWeaponTags::Damage, -WeaponDefinition->DamagePerShot);
			SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			Shot.bDamageApplied = Target->GetHealth() < PreviousHealth;
		}
	}
	PublishWeaponState(Previous);
	if (UDemoEquipmentManagerComponent* Manager = GetManager())
	{
		Manager->DispatchWeaponShot(Shot);
	}
	return EDemoWeaponResult::Success;
}

EDemoWeaponResult UDemoWeaponInstance::BeginReload(FGuid& ReloadId)
{
	ReloadId.Invalidate();
	const EDemoWeaponResult Result = CheckWeaponCommand();
	if (Result != EDemoWeaponResult::Success)
	{
		return Result;
	}
	if (WeaponState.bReloading)
	{
		return EDemoWeaponResult::Reloading;
	}
	const UDemoWeaponDefinition* WeaponDefinition = GetWeaponDefinition();
	if (WeaponState.MagazineAmmo >= WeaponDefinition->MagazineCapacity)
	{
		return EDemoWeaponResult::MagazineFull;
	}
	if (WeaponState.ReserveAmmo <= 0)
	{
		return EDemoWeaponResult::NoReserveAmmo;
	}
	TGuardValue<bool> Guard(bWeaponMutating, true);
	const FDemoWeaponState Previous = WeaponState;
	PendingReloadId = FGuid::NewGuid();
	ReloadId = PendingReloadId;
	ReloadController = GetPawn()->GetController();
	ReloadAbilitySystem = GetPawn()->FindComponentByClass<UPawnGasBindingComponent>()->GetAbilitySystem();
	WeaponState.bReloading = true;
	WeaponState.ReloadEndServerTime = GetWorld()->GetTimeSeconds() + WeaponDefinition->ReloadDuration;
	const FGuid ExpectedId = PendingReloadId;
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer,
	                                       FTimerDelegate::CreateWeakLambda(this,
	                                                                        [this, ExpectedId]()
	                                                                        {
		                                                                        if (PendingReloadId == ExpectedId)
		                                                                        {
			                                                                        FinishReload(
			                                                                            EDemoWeaponResult::Success);
		                                                                        }
	                                                                        }),
	                                       WeaponDefinition->ReloadDuration, false);
	PublishWeaponState(Previous);
	return EDemoWeaponResult::Success;
}

EDemoWeaponResult UDemoWeaponInstance::CancelReload(FGuid ReloadId)
{
	check(IsInGameThread());
	if (!GetPawn() || !GetPawn()->HasAuthority())
	{
		return EDemoWeaponResult::NotAuthority;
	}
	if (!ReloadId.IsValid() || PendingReloadId != ReloadId)
	{
		return EDemoWeaponResult::NotFound;
	}
	FinishReload(EDemoWeaponResult::Canceled);
	return EDemoWeaponResult::Success;
}

void UDemoWeaponInstance::FinishReload(EDemoWeaponResult Result)
{
	if (!PendingReloadId.IsValid())
	{
		return;
	}
	if (Result == EDemoWeaponResult::Success)
	{
		const APawn* OwnerPawn = GetPawn();
		const UPawnGasBindingComponent* Binding =
		    OwnerPawn ? OwnerPawn->FindComponentByClass<UPawnGasBindingComponent>() : nullptr;
		if (CheckWeaponCommand() != EDemoWeaponResult::Success || !OwnerPawn || !Binding ||
		    OwnerPawn->GetController() != ReloadController.Get() ||
		    Binding->GetAbilitySystem() != ReloadAbilitySystem.Get())
		{
			Result = EDemoWeaponResult::Canceled;
		}
	}
	TGuardValue<bool> Guard(bWeaponMutating, true);
	const FDemoWeaponState Previous = WeaponState;
	const FGuid FinishedId = PendingReloadId;
	PendingReloadId.Invalidate();
	ReloadController.Reset();
	ReloadAbilitySystem.Reset();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimer);
	}
	WeaponState.bReloading = false;
	WeaponState.ReloadEndServerTime = 0.0f;
	if (Result == EDemoWeaponResult::Success)
	{
		const int32 Count =
		    FMath::Min(GetWeaponDefinition()->MagazineCapacity - WeaponState.MagazineAmmo, WeaponState.ReserveAmmo);
		WeaponState.MagazineAmmo += Count;
		WeaponState.ReserveAmmo -= Count;
	}
	PublishWeaponState(Previous);
	ReloadFinished.Broadcast(FinishedId, Result);
}

void UDemoWeaponInstance::CancelWeaponActions()
{
	check(IsInGameThread());
	if (bCancelingActions)
	{
		return;
	}
	TGuardValue<bool> Guard(bCancelingActions, true);
	FinishReload(EDemoWeaponResult::Canceled);
	ActionsCanceled.Broadcast();
	APawn* OwnerPawn = GetPawn();
	const UDemoWeaponDefinition* WeaponDefinition = GetWeaponDefinition();
	USkeletalMeshComponent* Mesh = Nelaric::DemoEquipment::PresentationMesh(OwnerPawn);
	if (OwnerPawn && OwnerPawn->GetNetMode() != NM_DedicatedServer && Mesh && Mesh->GetAnimInstance() &&
	    WeaponDefinition && WeaponDefinition->FireMontage)
	{
		Mesh->GetAnimInstance()->Montage_Stop(0.1f, WeaponDefinition->FireMontage);
	}
	RefreshReloadPresentation();
}

void UDemoWeaponInstance::ResetAmmunition()
{
	check(IsInGameThread());
	if (!GetPawn() || !GetPawn()->HasAuthority() || bWeaponMutating || bCancelingActions || !GetWeaponDefinition())
	{
		return;
	}
	CancelWeaponActions();
	TGuardValue<bool> Guard(bWeaponMutating, true);
	const FDemoWeaponState Previous = WeaponState;
	InitializeWeaponState();
	NextAllowedFireTime = 0.0;
	PublishWeaponState(Previous);
}

Nelaric::DemoEquipment::FWeaponActionsCanceled& UDemoWeaponInstance::OnActionsCanceled()
{
	check(IsInGameThread());
	return ActionsCanceled;
}

Nelaric::DemoEquipment::FWeaponReloadFinished& UDemoWeaponInstance::OnReloadFinished()
{
	check(IsInGameThread());
	return ReloadFinished;
}

void UDemoWeaponInstance::ValidatePendingActions()
{
	const APawn* OwnerPawn = GetPawn();
	if (!OwnerPawn || !OwnerPawn->HasAuthority() || !WeaponState.bReloading)
	{
		return;
	}
	const ADemoCharacter* Character = Cast<ADemoCharacter>(OwnerPawn);
	const UPawnGasBindingComponent* Binding = OwnerPawn->FindComponentByClass<UPawnGasBindingComponent>();
	if (!IsActive() || !Binding || !Binding->IsReadyForActions() ||
	    OwnerPawn->GetController() != ReloadController.Get() ||
	    Binding->GetAbilitySystem() != ReloadAbilitySystem.Get() ||
	    (Character && (!Character->IsPoolActive() || !Character->IsAlive())))
	{
		CancelWeaponActions();
	}
}

void UDemoWeaponInstance::PublishWeaponState(const FDemoWeaponState& Previous)
{
	if (UDemoEquipmentManagerComponent* Manager = GetManager())
	{
		Manager->NotifyWeaponStateChanged();
	}
	RefreshReloadPresentation();
	if (GetPawn() && GetWorld() && !GetWorld()->bIsTearingDown)
	{
		OnWeaponStateChanged(Previous, WeaponState);
	}
}

void UDemoWeaponInstance::ApplyWeaponState(const FDemoWeaponState& NewState)
{
	const FDemoWeaponState Previous = WeaponState;
	WeaponState = NewState;
	RefreshReloadPresentation();
	if (Previous.MagazineAmmo != NewState.MagazineAmmo || Previous.ReserveAmmo != NewState.ReserveAmmo ||
	    Previous.bReloading != NewState.bReloading || Previous.ReloadEndServerTime != NewState.ReloadEndServerTime ||
	    Previous.ShotsFired != NewState.ShotsFired || Previous.LastFireServerTime != NewState.LastFireServerTime)
	{
		OnWeaponStateChanged(Previous, WeaponState);
	}
}

void UDemoWeaponInstance::RefreshReloadPresentation()
{
	APawn* OwnerPawn = GetPawn();
	if (!OwnerPawn || OwnerPawn->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	USkeletalMeshComponent* Mesh = Nelaric::DemoEquipment::PresentationMesh(OwnerPawn);
	UAnimInstance* Anim = Mesh ? Mesh->GetAnimInstance() : nullptr;
	const UDemoWeaponDefinition* WeaponDefinition = GetWeaponDefinition();
	UAnimMontage* Montage = WeaponDefinition ? WeaponDefinition->ReloadMontage.Get() : nullptr;
	const ADemoCharacter* Character = Cast<ADemoCharacter>(OwnerPawn);
	const bool bPlay = IsActive() && WeaponState.bReloading && GetReloadRemainingTime() > 0.0f && Anim && Montage &&
	                   (!Character || (Character->IsAlive() && Character->IsPoolActive()));
	if (!bPlay || Mesh != ReloadPresentationMesh.Get() || Montage != PlayingReloadMontage.Get())
	{
		USkeletalMeshComponent* OldMesh = ReloadPresentationMesh.Get();
		UAnimInstance* OldAnim = OldMesh ? OldMesh->GetAnimInstance() : nullptr;
		if (OldAnim && PlayingReloadMontage.IsValid())
		{
			OldAnim->Montage_Stop(0.1f, PlayingReloadMontage.Get());
		}
		ReloadPresentationMesh.Reset();
		PlayingReloadMontage.Reset();
	}
	if (bPlay && !PlayingReloadMontage.IsValid())
	{
		const float Rate = Montage->GetPlayLength() / WeaponDefinition->ReloadDuration;
		const float Position = FMath::Max(0.0f, WeaponDefinition->ReloadDuration - GetReloadRemainingTime()) * Rate;
		if (Rate > 0.0f && Anim->Montage_Play(Montage, Rate, EMontagePlayReturnType::MontageLength, Position) > 0.0f)
		{
			ReloadPresentationMesh = Mesh;
			PlayingReloadMontage = Montage;
		}
	}
}

void UDemoWeaponInstance::PresentShot(const FDemoWeaponShot& Shot, const FTransform& Muzzle)
{
	APawn* OwnerPawn = GetPawn();
	const UDemoWeaponDefinition* WeaponDefinition = Shot.Definition;
	if (!OwnerPawn || !WeaponDefinition || OwnerPawn->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (WeaponDefinition->FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(OwnerPawn, WeaponDefinition->FireSound, Muzzle.GetLocation());
	}
	USkeletalMeshComponent* Mesh = Nelaric::DemoEquipment::PresentationMesh(OwnerPawn);
	if (IsActive() && WeaponDefinition->FireMontage && Mesh && Mesh->GetAnimInstance())
	{
		Mesh->GetAnimInstance()->Montage_Play(WeaponDefinition->FireMontage);
	}
}
