// Copyright (c) 2026 Nelaric Contributors

/** @file DemoEquipmentDefinition.h
 * Defines shared equipment, weapon, and initial loadout configuration.
 */

#pragma once

#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

#include "DemoEquipmentDefinition.generated.h"

class UAnimInstance;
class UAnimMontage;
class UDemoEquipmentInstance;
class UGameplayEffect;
class USoundBase;

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

/// Shared hitscan configuration and local presentation for one weapon type.
UCLASS(MinimalAPI, BlueprintType)
class UDemoWeaponDefinition : public UDemoEquipmentDefinition
{
	GENERATED_BODY()

public:
	/// Checks finite combat values and an instant damage effect; game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Weapon")
	bool IsCombatConfigurationValid() const;

	/// Layer blueprint implementing the target pawn's weapon layer interface.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<UAnimInstance> ActiveAnimationLayer;

	/// Maximum magazine rounds; a newly equipped weapon starts full.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "1"))
	int32 MagazineCapacity = 30;

	/// Per-item reserve rounds assigned on equip or an explicit ammo reset.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "0"))
	int32 InitialReserveAmmo = 90;

	/// Minimum server seconds between accepted shots.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "0.02"))
	float FireInterval = 0.1f;

	/// Hold fire to repeat shots; false accepts one shot per press.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	bool bAutomatic = true;

	/// Maximum authority trace distance in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "1.0"))
	float Range = 10000.0f;

	/// Positive health reduction passed as negative Data.Weapon.Damage.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "0.0"))
	float DamagePerShot = 10.0f;

	/// Server seconds before reserve rounds move into the magazine.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat", meta = (ClampMin = "0.05"))
	float ReloadDuration = 2.0f;

	/// Blocking trace channel; target and cover must block this channel.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/// Instant effect consuming Data.Weapon.Damage; a native effect is default.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
	TSubclassOf<UGameplayEffect> DamageEffect;

	/// Optional character montage; the main animation graph needs its slot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
	TObjectPtr<UAnimMontage> FireMontage;

	/// Optional character montage, scaled to the authority reload duration.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
	TObjectPtr<UAnimMontage> ReloadMontage;

	/// Optional sound emitted locally for each accepted shot.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
	TObjectPtr<USoundBase> FireSound;

	/// Executed locally for accepted shots; None disables the fire cue.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation", meta = (Categories = "GameplayCue"))
	FGameplayTag FireGameplayCue;

	/// Executed locally for blocking hits, including cover; None disables it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation", meta = (Categories = "GameplayCue"))
	FGameplayTag ImpactGameplayCue;

	/// Socket on a cosmetic weapon mesh; missing sockets use its actor origin.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Presentation")
	FName MuzzleSocketName = "Muzzle";

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
