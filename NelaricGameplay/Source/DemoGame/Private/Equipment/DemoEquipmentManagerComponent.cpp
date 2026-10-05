// Copyright (c) 2026 Nelaric Contributors

#include "Equipment/DemoEquipmentManagerComponent.h"

#include "Animation/AnimInstance.h"
#include "AbilitySystemGlobals.h"
#include "Character/DemoCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "GameFramework/Pawn.h"
#include "GameplayCueManager.h"
#include "Templates/UnrealTemplate.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoEquipment, Log, All);

namespace Nelaric::DemoEquipment
{
static FTransform ResolveShotMuzzle(const FDemoWeaponShot& Shot, const UDemoWeaponInstance* Weapon,
                                    USceneComponent*& AttachComponent)
{
	AttachComponent = nullptr;
	FTransform Muzzle((Shot.TraceEnd - Shot.TraceStart).Rotation(), Shot.TraceStart);
	if (!Weapon || !Weapon->IsActive() || Weapon->GetWeaponDefinition() != Shot.Definition)
	{
		return Muzzle;
	}
	for (AActor* Visual : Weapon->GetVisualActors())
	{
		if (!IsValid(Visual))
		{
			continue;
		}
		Muzzle = Visual->GetActorTransform();
		TInlineComponentArray<USkeletalMeshComponent*> Meshes(Visual);
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && Mesh->DoesSocketExist(Shot.Definition->MuzzleSocketName))
			{
				AttachComponent = Mesh;
				return Mesh->GetSocketTransform(Shot.Definition->MuzzleSocketName);
			}
		}
	}
	return Muzzle;
}

static void ExecuteWeaponCues(APawn& Pawn, const FDemoWeaponShot& Shot, const FTransform& Muzzle,
                              USceneComponent* AttachComponent)
{
	FGameplayEffectContextHandle Context(UAbilitySystemGlobals::Get().AllocGameplayEffectContext());
	Context.AddInstigator(&Pawn, &Pawn);
	Context.AddSourceObject(Shot.Definition);
	Context.AddOrigin(Muzzle.GetLocation());
	Context.AddHitResult(Shot.Hit, true);
	FGameplayCueParameters FireParameters;
	FireParameters.EffectContext = Context;
	FireParameters.Instigator = &Pawn;
	FireParameters.EffectCauser = &Pawn;
	FireParameters.SourceObject = Shot.Definition;
	FireParameters.Location = Muzzle.GetLocation();
	FireParameters.Normal = (Shot.TraceEnd - Shot.TraceStart).GetSafeNormal();
	FireParameters.TargetAttachComponent = AttachComponent;
	FGameplayCueParameters ImpactParameters = FireParameters;
	ImpactParameters.Location = Shot.Hit.ImpactPoint;
	ImpactParameters.Normal = Shot.Hit.ImpactNormal;
	ImpactParameters.PhysicalMaterial = Shot.Hit.PhysMaterial;
	ImpactParameters.TargetAttachComponent.Reset();

	// The shot multicast already transported this event. Cue execution is local.
	if (Shot.Definition->FireGameplayCue.IsValid())
	{
		UGameplayCueManager::ExecuteGameplayCue_NonReplicated(&Pawn, Shot.Definition->FireGameplayCue, FireParameters);
	}
	if (IsValid(&Pawn) && IsValid(Shot.Definition) && Shot.Hit.bBlockingHit &&
	    Shot.Definition->ImpactGameplayCue.IsValid())
	{
		UGameplayCueManager::ExecuteGameplayCue_NonReplicated(&Pawn, Shot.Definition->ImpactGameplayCue,
		                                                      ImpactParameters);
	}
}
} // namespace Nelaric::DemoEquipment

UDemoEquipmentManagerComponent::UDemoEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

EDemoEquipmentResult UDemoEquipmentManagerComponent::CheckMutation() const
{
	check(IsInGameThread());
	const APawn* Pawn = GetPawn();
	if (!bStarted || bEnding || !Pawn || GetInitState() != Nelaric::EInitState::Ready)
	{
		return EDemoEquipmentResult::NotReady;
	}
	if (!Pawn->HasAuthority())
	{
		return EDemoEquipmentResult::NotAuthority;
	}
	return bMutating ? EDemoEquipmentResult::Busy : EDemoEquipmentResult::Success;
}

bool UDemoEquipmentManagerComponent::IsDefinitionValid(const UDemoEquipmentDefinition* Definition) const
{
	if (!IsValid(Definition) || Definition->Slot.IsNone() || !Definition->InstanceClass ||
	    Definition->InstanceClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return false;
	}
	const bool bWeaponClass = Definition->InstanceClass->IsChildOf(UDemoWeaponInstance::StaticClass());
	const UDemoWeaponDefinition* WeaponDefinition = Cast<UDemoWeaponDefinition>(Definition);
	if (bWeaponClass != (WeaponDefinition != nullptr) ||
	    (WeaponDefinition &&
	     (!WeaponDefinition->ActiveAnimationLayer || !WeaponDefinition->IsCombatConfigurationValid())))
	{
		return false;
	}
	for (const FDemoEquipmentVisual& Visual : Definition->Visuals)
	{
		if (!Visual.ActorClass || Visual.ActorClass->HasAnyClassFlags(CLASS_Abstract) ||
		    Visual.MeshComponentName.IsNone() || Visual.ActorClass->GetDefaultObject<AActor>()->GetIsReplicated())
		{
			return false;
		}
	}
	return true;
}

EDemoEquipmentResult UDemoEquipmentManagerComponent::Equip(UDemoEquipmentDefinition* Definition, FGuid& EquipmentId)
{
	EquipmentId.Invalidate();
	const EDemoEquipmentResult Result = CheckMutation();
	if (Result != EDemoEquipmentResult::Success)
	{
		return Result;
	}
	if (!IsDefinitionValid(Definition))
	{
		return EDemoEquipmentResult::InvalidDefinition;
	}
	if (FindEquipmentInSlot(Definition->Slot))
	{
		return EDemoEquipmentResult::SlotOccupied;
	}
	TGuardValue<bool> Guard(bMutating, true);
	UDemoEquipmentInstance* Instance = NewObject<UDemoEquipmentInstance>(this, Definition->InstanceClass);
	EquipmentId = FGuid::NewGuid();
	Instances.Add(Instance);
	Instance->Initialize(GetPawn(), Definition, EquipmentId);
	if (bEnding)
	{
		EquipmentId.Invalidate();
		return EDemoEquipmentResult::NotReady;
	}
	PublishState();
	return EDemoEquipmentResult::Success;
}

EDemoEquipmentResult UDemoEquipmentManagerComponent::Unequip(FGuid EquipmentId)
{
	const EDemoEquipmentResult Result = CheckMutation();
	if (Result != EDemoEquipmentResult::Success)
	{
		return Result;
	}
	UDemoEquipmentInstance* Instance = FindEquipment(EquipmentId);
	if (!Instance)
	{
		return EDemoEquipmentResult::NotFound;
	}
	TGuardValue<bool> Guard(bMutating, true);
	if (ActiveEquipmentId == EquipmentId)
	{
		ActiveEquipmentId.Invalidate();
	}
	Instances.Remove(Instance);
	Instance->Remove();
	PublishState();
	return bEnding ? EDemoEquipmentResult::NotReady : EDemoEquipmentResult::Success;
}

EDemoEquipmentResult UDemoEquipmentManagerComponent::ActivateEquipment(FGuid EquipmentId)
{
	const EDemoEquipmentResult Result = CheckMutation();
	if (Result != EDemoEquipmentResult::Success)
	{
		return Result;
	}
	UDemoEquipmentInstance* Instance = FindEquipment(EquipmentId);
	if (!Instance)
	{
		return EDemoEquipmentResult::NotFound;
	}
	if (ActiveEquipmentId == EquipmentId)
	{
		return EDemoEquipmentResult::Success;
	}
	if (const UDemoWeaponInstance* Weapon = Cast<UDemoWeaponInstance>(Instance); Weapon && !Weapon->CanActivate())
	{
		return EDemoEquipmentResult::InvalidAnimation;
	}
	TGuardValue<bool> Guard(bMutating, true);
	UDemoEquipmentInstance* Previous = GetActiveEquipment();
	ActiveEquipmentId.Invalidate();
	if (Previous)
	{
		Previous->SetActive(false);
	}
	if (bEnding)
	{
		return EDemoEquipmentResult::NotReady;
	}
	ActiveEquipmentId = EquipmentId;
	Instance->SetActive(true);
	PublishState();
	return bEnding ? EDemoEquipmentResult::NotReady : EDemoEquipmentResult::Success;
}

EDemoEquipmentResult UDemoEquipmentManagerComponent::DeactivateEquipment()
{
	const EDemoEquipmentResult Result = CheckMutation();
	if (Result != EDemoEquipmentResult::Success)
	{
		return Result;
	}
	UDemoEquipmentInstance* Previous = GetActiveEquipment();
	if (!Previous)
	{
		return EDemoEquipmentResult::Success;
	}
	TGuardValue<bool> Guard(bMutating, true);
	ActiveEquipmentId.Invalidate();
	Previous->SetActive(false);
	PublishState();
	return bEnding ? EDemoEquipmentResult::NotReady : EDemoEquipmentResult::Success;
}

UDemoEquipmentInstance* UDemoEquipmentManagerComponent::FindEquipment(FGuid EquipmentId) const
{
	check(IsInGameThread());
	for (UDemoEquipmentInstance* Instance : Instances)
	{
		if (IsValid(Instance) && Instance->GetEquipmentId() == EquipmentId)
		{
			return Instance;
		}
	}
	return nullptr;
}

UDemoEquipmentInstance* UDemoEquipmentManagerComponent::FindEquipmentInSlot(FName Slot) const
{
	check(IsInGameThread());
	for (UDemoEquipmentInstance* Instance : Instances)
	{
		if (IsValid(Instance) && Instance->GetDefinition() && Instance->GetDefinition()->Slot == Slot)
		{
			return Instance;
		}
	}
	return nullptr;
}

UDemoEquipmentInstance* UDemoEquipmentManagerComponent::GetActiveEquipment() const
{
	return FindEquipment(ActiveEquipmentId);
}

UDemoWeaponInstance* UDemoEquipmentManagerComponent::GetActiveWeapon() const
{
	return Cast<UDemoWeaponInstance>(GetActiveEquipment());
}

void UDemoEquipmentManagerComponent::CancelWeaponActions()
{
	check(IsInGameThread());
	if (!GetPawn() || !GetPawn()->HasAuthority())
	{
		return;
	}
	{
		TGuardValue<bool> Guard(bMutating, true);
		const TArray<TObjectPtr<UDemoEquipmentInstance>> Current = Instances;
		for (UDemoEquipmentInstance* Instance : Current)
		{
			if (UDemoWeaponInstance* Weapon = Cast<UDemoWeaponInstance>(Instance))
			{
				Weapon->CancelWeaponActions();
			}
		}
	}
	if (bWeaponStateDirty && !bMutating)
	{
		PublishState();
	}
}

void UDemoEquipmentManagerComponent::ResetWeaponAmmunition()
{
	check(IsInGameThread());
	if (CheckMutation() != EDemoEquipmentResult::Success)
	{
		return;
	}
	TGuardValue<bool> Guard(bMutating, true);
	const TArray<TObjectPtr<UDemoEquipmentInstance>> Current = Instances;
	for (UDemoEquipmentInstance* Instance : Current)
	{
		if (UDemoWeaponInstance* Weapon = Cast<UDemoWeaponInstance>(Instance))
		{
			Weapon->ResetAmmunition();
		}
	}
	PublishState();
}

void UDemoEquipmentManagerComponent::NotifyWeaponStateChanged()
{
	bWeaponStateDirty = true;
	if (!bMutating)
	{
		PublishState();
	}
}

void UDemoEquipmentManagerComponent::DispatchWeaponShot(const FDemoWeaponShot& Shot)
{
	if (!bEnding && GetPawn() && GetPawn()->HasAuthority())
	{
		MulticastWeaponShot(Shot);
	}
}

void UDemoEquipmentManagerComponent::MulticastWeaponShot_Implementation(const FDemoWeaponShot& Shot)
{
	APawn* Pawn = GetPawn();
	if (bEnding || !IsValid(Pawn) || !IsValid(Shot.Definition) || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UDemoWeaponInstance* Weapon = Cast<UDemoWeaponInstance>(FindEquipment(Shot.EquipmentId));
	USceneComponent* AttachComponent = nullptr;
	const FTransform Muzzle = Nelaric::DemoEquipment::ResolveShotMuzzle(Shot, Weapon, AttachComponent);
	if (Weapon)
	{
		Weapon->PresentShot(Shot, Muzzle);
	}
	if (!bEnding && IsValid(Pawn) && IsValid(Shot.Definition))
	{
		Nelaric::DemoEquipment::ExecuteWeaponCues(*Pawn, Shot, Muzzle, AttachComponent);
	}
	if (!bEnding)
	{
		OnWeaponShot(Shot);
	}
}

TArray<UDemoEquipmentInstance*> UDemoEquipmentManagerComponent::GetEquipment() const
{
	check(IsInGameThread());
	TArray<UDemoEquipmentInstance*> Result;
	for (UDemoEquipmentInstance* Instance : Instances)
	{
		if (IsValid(Instance))
		{
			Result.Add(Instance);
		}
	}
	return Result;
}

Nelaric::DemoEquipment::FStateChanged& UDemoEquipmentManagerComponent::OnStateChanged()
{
	check(IsInGameThread());
	return StateChanged;
}

void UDemoEquipmentManagerComponent::PublishState()
{
	APawn* Pawn = GetPawn();
	if (bEnding || !Pawn || !Pawn->HasAuthority())
	{
		return;
	}
	Snapshot.Entries.Reset(Instances.Num());
	for (UDemoEquipmentInstance* Instance : Instances)
	{
		FDemoEquipmentEntry& Entry = Snapshot.Entries.AddDefaulted_GetRef();
		Entry.EquipmentId = Instance->GetEquipmentId();
		Entry.Definition = Instance->GetDefinition();
		if (const UDemoWeaponInstance* Weapon = Cast<UDemoWeaponInstance>(Instance))
		{
			Entry.WeaponState = Weapon->GetWeaponState();
		}
	}
	Snapshot.ActiveEquipmentId = ActiveEquipmentId;
	bWeaponStateDirty = false;
	++Snapshot.Revision;
	Pawn->FlushNetDormancy();
	Pawn->ForceNetUpdate();
	StateChanged.Broadcast();
}

bool UDemoEquipmentManagerComponent::ApplySnapshot()
{
	const APawn* Pawn = GetPawn();
	if (!bStarted || bEnding || bMutating || !Pawn || Pawn->HasAuthority() ||
	    GetInitState() != Nelaric::EInitState::Ready)
	{
		return false;
	}
	if (LastAppliedRevision == Snapshot.Revision)
	{
		return true;
	}
	TSet<FGuid> Ids;
	TSet<FName> Slots;
	for (const FDemoEquipmentEntry& Entry : Snapshot.Entries)
	{
		if (!Entry.EquipmentId.IsValid() || Ids.Contains(Entry.EquipmentId) || !IsDefinitionValid(Entry.Definition) ||
		    Slots.Contains(Entry.Definition->Slot))
		{
			return false;
		}
		Ids.Add(Entry.EquipmentId);
		Slots.Add(Entry.Definition->Slot);
	}
	if (Snapshot.ActiveEquipmentId.IsValid() && !Ids.Contains(Snapshot.ActiveEquipmentId))
	{
		return false;
	}
	TGuardValue<bool> Guard(bMutating, true);
	if (ActiveEquipmentId != Snapshot.ActiveEquipmentId)
	{
		UDemoEquipmentInstance* Previous = GetActiveEquipment();
		ActiveEquipmentId.Invalidate();
		if (Previous)
		{
			Previous->SetActive(false);
		}
	}
	const TArray<TObjectPtr<UDemoEquipmentInstance>> OldInstances = Instances;
	for (UDemoEquipmentInstance* Instance : OldInstances)
	{
		const FDemoEquipmentEntry* Entry =
		    Snapshot.Entries.FindByPredicate([Instance](const FDemoEquipmentEntry& Candidate)
		                                     { return Candidate.EquipmentId == Instance->GetEquipmentId(); });
		if (!Entry || Entry->Definition != Instance->GetDefinition())
		{
			Instances.Remove(Instance);
			Instance->Remove();
		}
	}
	for (const FDemoEquipmentEntry& Entry : Snapshot.Entries)
	{
		if (bEnding)
		{
			return false;
		}
		if (!FindEquipment(Entry.EquipmentId))
		{
			UDemoEquipmentInstance* Instance = NewObject<UDemoEquipmentInstance>(this, Entry.Definition->InstanceClass);
			Instances.Add(Instance);
			Instance->Initialize(GetPawn(), Entry.Definition, Entry.EquipmentId);
		}
	}
	if (bEnding)
	{
		return false;
	}
	ActiveEquipmentId = Snapshot.ActiveEquipmentId;
	if (UDemoEquipmentInstance* Active = GetActiveEquipment())
	{
		Active->SetActive(true);
	}
	for (const FDemoEquipmentEntry& Entry : Snapshot.Entries)
	{
		if (UDemoWeaponInstance* Weapon = Cast<UDemoWeaponInstance>(FindEquipment(Entry.EquipmentId)))
		{
			Weapon->ApplyWeaponState(Entry.WeaponState);
		}
		if (bEnding)
		{
			return false;
		}
	}
	LastAppliedRevision = Snapshot.Revision;
	return true;
}

void UDemoEquipmentManagerComponent::OnRep_Snapshot()
{
	ApplySnapshot();
}

void UDemoEquipmentManagerComponent::BeginPlay()
{
	Super::BeginPlay();
	bStarted = true;
	APawn* Pawn = GetPawn();
	if (!Pawn || !Pawn->HasAuthority())
	{
		ApplySnapshot();
		return;
	}
	ApplyInitialLoadout();
}

void UDemoEquipmentManagerComponent::ApplyInitialLoadout()
{
	const APawn* Pawn = GetPawn();
	if (bInitialLoadoutApplied || bEnding || !bStarted || !Pawn || !Pawn->HasAuthority() ||
	    GetInitState() != Nelaric::EInitState::Ready)
	{
		return;
	}
	bInitialLoadoutApplied = true;
	if (InitialLoadout)
	{
		for (UDemoEquipmentDefinition* Definition : InitialLoadout->Equipment)
		{
			FGuid Id;
			const EDemoEquipmentResult Result = Equip(Definition, Id);
			if (Result != EDemoEquipmentResult::Success)
			{
				UE_LOG(LogDemoEquipment, Warning, TEXT("Initial equipment '%s' failed: %d."), *GetNameSafe(Definition),
				       static_cast<int32>(Result));
			}
		}
		if (!InitialLoadout->InitialActiveSlot.IsNone())
		{
			UDemoEquipmentInstance* Active = FindEquipmentInSlot(InitialLoadout->InitialActiveSlot);
			const EDemoEquipmentResult Result =
			    Active ? ActivateEquipment(Active->GetEquipmentId()) : EDemoEquipmentResult::NotFound;
			if (Result != EDemoEquipmentResult::Success)
			{
				UE_LOG(LogDemoEquipment, Warning, TEXT("Initial active slot '%s' failed: %d."),
				       *InitialLoadout->InitialActiveSlot.ToString(), static_cast<int32>(Result));
			}
		}
	}
	PublishState();
}

void UDemoEquipmentManagerComponent::ClearInstances()
{
	TGuardValue<bool> Guard(bMutating, true);
	ActiveEquipmentId.Invalidate();
	TArray<TObjectPtr<UDemoEquipmentInstance>> OldInstances = MoveTemp(Instances);
	Instances.Empty();
	for (UDemoEquipmentInstance* Instance : OldInstances)
	{
		if (IsValid(Instance))
		{
			Instance->Remove();
		}
	}
}

void UDemoEquipmentManagerComponent::Shutdown()
{
	if (!bEnding)
	{
		bEnding = true;
		ClearInstances();
	}
}

void UDemoEquipmentManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Shutdown();
	Super::EndPlay(EndPlayReason);
}

void UDemoEquipmentManagerComponent::OnUnregister()
{
	Shutdown();
	Super::OnUnregister();
}

void UDemoEquipmentManagerComponent::CancelInitGenerationWork()
{
	CancelWeaponActions();
	Super::CancelInitGenerationWork();
}

void UDemoEquipmentManagerComponent::RefreshEquipmentPresentation()
{
	if (bEnding)
	{
		return;
	}
	TGuardValue<bool> Guard(bMutating, true);
	const TArray<TObjectPtr<UDemoEquipmentInstance>> Current = Instances;
	for (UDemoEquipmentInstance* Instance : Current)
	{
		if (!IsValid(Instance) || bEnding)
		{
			break;
		}
		Instance->RefreshVisuals();
		if (UDemoWeaponInstance* Weapon = Cast<UDemoWeaponInstance>(Instance))
		{
			Weapon->RefreshReloadPresentation();
			const ADemoCharacter* Character = GetPawn<ADemoCharacter>();
			if (Character && !Character->IsPoolActive())
			{
				Weapon->CancelWeaponActions();
			}
		}
	}
}

void UDemoEquipmentManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                   FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ApplyInitialLoadout();
	ApplySnapshot();
	if (!bEnding)
	{
		TGuardValue<bool> Guard(bMutating, true);
		const TArray<TObjectPtr<UDemoEquipmentInstance>> CurrentInstances = Instances;
		for (UDemoEquipmentInstance* Instance : CurrentInstances)
		{
			if (bEnding || !IsValid(Instance))
			{
				break;
			}
			Instance->RefreshVisuals();
			if (UDemoWeaponInstance* Weapon = Cast<UDemoWeaponInstance>(Instance))
			{
				Weapon->ValidatePendingActions();
				Weapon->RefreshAnimationLayer();
				Weapon->RefreshReloadPresentation();
			}
		}
	}
	if (bWeaponStateDirty && !bMutating)
	{
		PublishState();
	}
}

void UDemoEquipmentManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDemoEquipmentManagerComponent, Snapshot);
}
