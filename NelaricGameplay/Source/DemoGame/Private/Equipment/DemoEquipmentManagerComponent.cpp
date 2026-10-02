// Copyright (c) 2026 Nelaric Contributors

#include "Equipment/DemoEquipmentManagerComponent.h"

#include "Animation/AnimInstance.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "GameFramework/Pawn.h"
#include "Templates/UnrealTemplate.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoEquipment, Log, All);

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
	if (bWeaponClass != (WeaponDefinition != nullptr) || (WeaponDefinition && !WeaponDefinition->ActiveAnimationLayer))
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
		Snapshot.Entries.Add({Instance->GetEquipmentId(), Instance->GetDefinition()});
	}
	Snapshot.ActiveEquipmentId = ActiveEquipmentId;
	++Snapshot.Revision;
	Pawn->FlushNetDormancy();
	Pawn->ForceNetUpdate();
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
				Weapon->RefreshAnimationLayer();
			}
		}
	}
}

void UDemoEquipmentManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDemoEquipmentManagerComponent, Snapshot);
}
