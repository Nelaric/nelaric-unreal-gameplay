// Copyright (c) 2026 Nelaric Contributors

#include "Equipment/DemoEquipmentInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Equipment/DemoEquipmentDefinition.h"
#include "Equipment/DemoPawnAnimationLayerComponent.h"
#include "GameFramework/Pawn.h"

FGuid UDemoEquipmentInstance::GetEquipmentId() const
{
	check(IsInGameThread());
	return EquipmentId;
}

UDemoEquipmentDefinition* UDemoEquipmentInstance::GetDefinition() const
{
	check(IsInGameThread());
	return Definition;
}

APawn* UDemoEquipmentInstance::GetPawn() const
{
	check(IsInGameThread());
	APawn* OwnerPawn = Pawn.Get();
	return OwnerPawn && !OwnerPawn->IsActorBeingDestroyed() ? OwnerPawn : nullptr;
}

bool UDemoEquipmentInstance::IsActive() const
{
	check(IsInGameThread());
	return bActive && !bRemoved;
}

TArray<AActor*> UDemoEquipmentInstance::GetVisualActors() const
{
	check(IsInGameThread());
	TArray<AActor*> Result;
	for (AActor* Actor : VisualActors)
	{
		if (IsValid(Actor))
		{
			Result.Add(Actor);
		}
	}
	return Result;
}

UWorld* UDemoEquipmentInstance::GetWorld() const
{
	const APawn* OwnerPawn = Pawn.Get();
	return OwnerPawn ? OwnerPawn->GetWorld() : nullptr;
}

void UDemoEquipmentInstance::Initialize(APawn* InPawn, UDemoEquipmentDefinition* InDefinition, FGuid InId)
{
	Pawn = InPawn;
	Definition = InDefinition;
	EquipmentId = InId;
	VisualActors.SetNum(Definition->Visuals.Num());
	RefreshVisuals();
	if (!bRemoved)
	{
		OnEquipped();
	}
}

void UDemoEquipmentInstance::SetActive(bool bNewActive)
{
	if (bRemoved || bActive == bNewActive)
	{
		return;
	}
	bActive = bNewActive;
	if (UDemoWeaponInstance* Weapon = Cast<UDemoWeaponInstance>(this))
	{
		if (bActive)
		{
			Weapon->RefreshAnimationLayer();
		}
		else
		{
			Weapon->ReleaseAnimationLayer();
		}
	}
	RefreshVisuals();
	if (bRemoved)
	{
		return;
	}
	if (bActive)
	{
		OnActivated();
	}
	else
	{
		OnDeactivated();
	}
}

void UDemoEquipmentInstance::Remove()
{
	if (bRemoved)
	{
		return;
	}
	SetActive(false);
	bRemoved = true;
	OnUnequipped();
	DestroyVisuals();
	Definition = nullptr;
	Pawn.Reset();
}

void UDemoEquipmentInstance::RefreshVisuals()
{
	APawn* OwnerPawn = GetPawn();
	UWorld* World = GetWorld();
	if (bRemoved || !OwnerPawn || !World || !Definition || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	TInlineComponentArray<USkeletalMeshComponent*> Meshes;
	OwnerPawn->GetComponents(Meshes);
	for (int32 Index = 0; Index < Definition->Visuals.Num(); ++Index)
	{
		const FDemoEquipmentVisual& Visual = Definition->Visuals[Index];
		USkeletalMeshComponent* TargetMesh = nullptr;
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (IsValid(Mesh) && Mesh->GetFName() == Visual.MeshComponentName)
			{
				TargetMesh = Mesh;
				break;
			}
		}
		AActor* Actor = VisualActors[Index];
		if (!TargetMesh || (!Visual.SocketName.IsNone() && !TargetMesh->DoesSocketExist(Visual.SocketName)))
		{
			if (IsValid(Actor))
			{
				Actor->SetActorHiddenInGame(true);
			}
			continue;
		}
		if (!IsValid(Actor))
		{
			FActorSpawnParameters Parameters;
			Parameters.Owner = OwnerPawn;
			Parameters.Instigator = OwnerPawn;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Actor = World->SpawnActor<AActor>(Visual.ActorClass, TargetMesh->GetComponentTransform(), Parameters);
			if (bRemoved)
			{
				if (IsValid(Actor))
				{
					Actor->Destroy();
				}
				return;
			}
			VisualActors[Index] = Actor;
			if (!Actor)
			{
				continue;
			}
			Actor->SetActorEnableCollision(false);
		}
		if (!Actor->GetRootComponent())
		{
			Actor->SetActorHiddenInGame(true);
			continue;
		}
		if (Actor->GetRootComponent()->GetAttachParent() != TargetMesh ||
		    Actor->GetRootComponent()->GetAttachSocketName() != Visual.SocketName)
		{
			Actor->AttachToComponent(TargetMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			                         Visual.SocketName);
			Actor->SetActorRelativeTransform(Visual.RelativeTransform);
		}
		Actor->SetActorHiddenInGame(Visual.bActiveOnly && !bActive);
	}
}

void UDemoEquipmentInstance::DestroyVisuals()
{
	for (AActor* Actor : VisualActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	VisualActors.Empty();
}

UDemoWeaponDefinition* UDemoWeaponInstance::GetWeaponDefinition() const
{
	return Cast<UDemoWeaponDefinition>(GetDefinition());
}

bool UDemoWeaponInstance::CanActivate() const
{
	const APawn* OwnerPawn = GetPawn();
	const UDemoWeaponDefinition* WeaponDefinition = GetWeaponDefinition();
	if (!OwnerPawn || !WeaponDefinition || !WeaponDefinition->ActiveAnimationLayer)
	{
		return false;
	}
	if (OwnerPawn->GetNetMode() == NM_DedicatedServer)
	{
		return true;
	}
	const UDemoPawnAnimationLayerComponent* Provider =
	    OwnerPawn->FindComponentByClass<UDemoPawnAnimationLayerComponent>();
	return Provider && Provider->CanUseLayer(WeaponDefinition->ActiveAnimationLayer);
}

void UDemoWeaponInstance::RefreshAnimationLayer()
{
	if (!IsActive())
	{
		return;
	}
	APawn* OwnerPawn = GetPawn();
	const UDemoWeaponDefinition* WeaponDefinition = GetWeaponDefinition();
	if (!OwnerPawn || !WeaponDefinition || OwnerPawn->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UDemoPawnAnimationLayerComponent* Provider = OwnerPawn->FindComponentByClass<UDemoPawnAnimationLayerComponent>();
	if (AnimationProvider.Get() == Provider && Provider && Provider->HasLayerRequest(AnimationHandle))
	{
		return;
	}
	ReleaseAnimationLayer();
	if (Provider)
	{
		AnimationHandle = Provider->AcquireLayer(this, WeaponDefinition->ActiveAnimationLayer);
		AnimationProvider = Provider;
	}
}

void UDemoWeaponInstance::ReleaseAnimationLayer()
{
	if (UDemoPawnAnimationLayerComponent* Provider = AnimationProvider.Get())
	{
		Provider->ReleaseLayer(AnimationHandle);
	}
	AnimationProvider.Reset();
	AnimationHandle.Invalidate();
}
