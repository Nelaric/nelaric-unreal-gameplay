// Copyright (c) 2026 Nelaric Contributors

/** @file DemoWeaponTypes.h Defines weapon results and replicated state. */
#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetSerialization.h"
#include "DemoWeaponTypes.generated.h"

class UDemoWeaponDefinition;

/// Result of a game-thread weapon command or reload completion.
UENUM(BlueprintType)
enum class EDemoWeaponResult : uint8
{
	/// The shot was committed or the reload was accepted or completed.
	Success,
	/// Only the owning pawn's authority may change weapon state.
	NotAuthority,
	/// The pawn, equipment, ability system or world is not ready.
	NotReady,
	/// A weapon state callback attempted a reentrant command.
	Busy,
	/// The weapon is not the pawn's selected equipment.
	Inactive,
	/// The pawn is dead or parked in its character pool.
	Unavailable,
	/// The weapon definition contains invalid combat configuration.
	InvalidDefinition,
	/// A previous accepted shot still occupies the firing interval.
	RateLimited,
	/// The magazine is empty.
	EmptyMagazine,
	/// The weapon is already reloading.
	Reloading,
	/// The magazine has no room for more ammunition.
	MagazineFull,
	/// No reserve ammunition remains.
	NoReserveAmmo,
	/// The supplied reload identity no longer names an active operation.
	NotFound,
	/// An accepted reload ended without moving ammunition.
	Canceled,
};

/// Authority-owned per-item data mirrored into local weapon instances.
USTRUCT(BlueprintType)
struct FDemoWeaponState
{
	GENERATED_BODY()

	/// Remaining rounds in this item's magazine; never negative.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 MagazineAmmo = 0;

	/// Remaining reserve rounds for this item; never negative.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 ReserveAmmo = 0;

	/// Whether one accepted reload is still pending.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bReloading = false;

	/// Reload deadline in server world seconds; zero while not reloading.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	float ReloadEndServerTime = 0.0f;

	/// Accepted shots since equip or reset, including misses; saturates.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	int32 ShotsFired = 0;
};

/// Cosmetic event for one authority-accepted shot, independent of snapshots.
USTRUCT(BlueprintType)
struct FDemoWeaponShot
{
	GENERATED_BODY()

	/// Stable identity of the firing equipment on its pawn.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	FGuid EquipmentId;

	/// Shared definition, available even if the local instance was removed.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UDemoWeaponDefinition> Definition;

	/// Authority view origin in world centimeters.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	FVector_NetQuantize TraceStart;

	/// Impact position or the full-range endpoint on a miss.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	FVector_NetQuantize TraceEnd;

	/// Blocking collision result; empty on a miss.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	FHitResult Hit;

	/// Whether the hit reduced a live target's health on authority.
	UPROPERTY(BlueprintReadOnly, Category = "Weapon")
	bool bDamageApplied = false;
};

namespace Nelaric::DemoEquipment
{
/// Synchronous notification that all pending actions on an item must stop.
DECLARE_MULTICAST_DELEGATE(FWeaponActionsCanceled);
/// Exactly one terminal notification for each accepted reload identity.
DECLARE_MULTICAST_DELEGATE_TwoParams(FWeaponReloadFinished, FGuid, EDemoWeaponResult);
} // namespace Nelaric::DemoEquipment
