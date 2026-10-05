// Copyright (c) 2026 Nelaric Contributors

/** @file DemoEquipmentTypes.h
 * Defines equipment operation results and replicated state snapshots.
 */

#pragma once

#include "CoreMinimal.h"
#include "Equipment/DemoWeaponTypes.h"

#include "DemoEquipmentTypes.generated.h"

class UDemoEquipmentDefinition;

namespace Nelaric::DemoEquipment
{
/// Authority game-thread notification after equipment state is committed.
DECLARE_MULTICAST_DELEGATE(FStateChanged);
} // namespace Nelaric::DemoEquipment

/// Outcome of a synchronous game-thread equipment operation.
UENUM(BlueprintType)
enum class EDemoEquipmentResult : uint8
{
	/// The requested state is committed, including an idempotent activation.
	Success,
	/// Only the pawn authority may change equipment gameplay state.
	NotAuthority,
	/// The manager has not started or is ending.
	NotReady,
	/// A lifecycle callback attempted to mutate an operation in progress.
	Busy,
	/// The definition, slot, instance class, or visual configuration is invalid.
	InvalidDefinition,
	/// Another item already occupies the definition's slot.
	SlotOccupied,
	/// No current equipment has the requested identifier.
	NotFound,
	/// The local weapon animation provider or configured layer is invalid.
	InvalidAnimation,
};

/// Identity and shared asset of one equipped item; mutable state is separate.
USTRUCT()
struct FDemoEquipmentEntry
{
	GENERATED_BODY()

	/// Server-issued identity preserved across local presentation rebuilds.
	UPROPERTY()
	FGuid EquipmentId;

	/// Shared authored asset; must be included in the cooked game.
	UPROPERTY()
	TObjectPtr<UDemoEquipmentDefinition> Definition;

	/// Per-item combat state; ignored for ordinary equipment definitions.
	UPROPERTY()
	FDemoWeaponState WeaponState;
};

/// One consistent authority snapshot, replicated by the equipment manager.
USTRUCT()
struct FDemoEquipmentSnapshot
{
	GENERATED_BODY()

	/// Equipped items, each with a unique slot and identifier.
	UPROPERTY()
	TArray<FDemoEquipmentEntry> Entries;

	/// Current active item; an invalid GUID means no active equipment.
	UPROPERTY()
	FGuid ActiveEquipmentId;

	/// Monotonic change counter used to suppress duplicate local application.
	UPROPERTY()
	uint32 Revision = 0;
};
