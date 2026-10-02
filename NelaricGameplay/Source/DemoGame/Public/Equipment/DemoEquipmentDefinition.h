// Copyright (c) 2026 Nelaric Contributors

/** @file DemoEquipmentDefinition.h
 * Defines shared equipment, weapon, and initial loadout configuration.
 */

#pragma once

#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"

#include "DemoEquipmentDefinition.generated.h"

class UAnimInstance;
class UDemoEquipmentInstance;

/// One local cosmetic actor attached to a named pawn skeletal mesh.
USTRUCT(BlueprintType)
struct FDemoEquipmentVisual
{
	GENERATED_BODY()

	/// Non-replicated cosmetic actor class; no gameplay authority is implied.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<AActor> ActorClass;

	/// Exact component object name on the pawn, not a socket or display name.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FName MeshComponentName = TEXT("CharacterMesh0");

	/// Attachment socket; None attaches to the mesh origin.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FName SocketName;

	/// Transform relative to the selected socket, in centimeters and degrees.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FTransform RelativeTransform = FTransform::Identity;

	/// Hide this actor while its equipment is inactive.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	bool bActiveOnly = true;
};

/** @brief Immutable authored configuration shared by equipment instances.
 * @details The asset does not contain
 * per-item mutable state. Assign one
 * unique slot per equipped item. Access equipment assets on the game thread.
 */
UCLASS(MinimalAPI, BlueprintType)
class UDemoEquipmentDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/// Concrete instance type created independently on each machine.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<UDemoEquipmentInstance> InstanceClass;

	/// Occupied slot; None is invalid, and each pawn has one item per slot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FName Slot;

	/// Cosmetic actors spawned locally; dedicated servers skip these.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TArray<FDemoEquipmentVisual> Visuals;

public:
	UDemoEquipmentDefinition();
};

/// Equipment specialization supplying the active weapon animation layer.
UCLASS(MinimalAPI, BlueprintType)
class UDemoWeaponDefinition : public UDemoEquipmentDefinition
{
	GENERATED_BODY()

public:
	/// Layer blueprint implementing the target pawn's weapon layer interface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<UAnimInstance> ActiveAnimationLayer;

public:
	UDemoWeaponDefinition();
};

/// Server-authored initial equipment applied once when the manager starts.
UCLASS(MinimalAPI, BlueprintType)
class UDemoEquipmentLoadout : public UDataAsset
{
	GENERATED_BODY()

public:
	/// Definitions to equip; slots must be distinct and definitions valid.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TArray<TObjectPtr<UDemoEquipmentDefinition>> Equipment;

	/// Initially active slot; None leaves all equipped items inactive.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FName InitialActiveSlot;
};
