// Copyright (c) 2026 Nelaric Contributors

/** @file PawnInitializationConfig.h
 * Declares an authored configuration asset for pawn initialization.
 */

#pragma once

#include "Engine/DataAsset.h"
#include "Components/ActorComponent.h"

#include "PawnInitializationConfig.generated.h"

/** @brief One component created and coordinated for a pawn.
 * @details IDs are unique within the asset and remain stable across edits.
 * Dependencies name other entries in this asset, including cyclic groups.
 * Non-replicated entries are created locally under the same ID. Listen
 * servers and standalone worlds create the
 * union of both creation flags.
 * Replicated entries are created only by authority; clients adopt their
 * replicated
 * references before resolving the local dependency graph.
 * Init State and Generation remain local, even for
 * replicated components.
 */
USTRUCT(BlueprintType)
struct FPawnInitializationEntry
{
	GENERATED_BODY()

	/// Stable identity used for dependency references and instance naming.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Initialization")
	FName ComponentId;

	/// Concrete component class created for the pawn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Initialization")
	TSubclassOf<UActorComponent> ComponentClass;

	/// Create this component on authority, including standalone play.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Initialization")
	bool bCreateOnAuthority = true;

	/// Create locally on clients, or adopt the authority's replicated instance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Initialization")
	bool bCreateOnClient = true;

	/** @brief Replicate the authority-created component through its pawn.
	 * @details Requires both creation flags.
	 * Networked pawns must replicate.
	 * Clients never create a duplicate. False preserves local-only creation.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Initialization")
	bool bReplicateComponent = false;

	/// Include this component in the local pawn readiness aggregate.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Initialization")
	bool bRequiredForPawnReady = true;

	/// IDs of components that must reach Ready before this entry enters Ready.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Initialization")
	TArray<FName> DependencyIds;
};

/// Authored component creation and readiness contract for a pawn.
UCLASS(MinimalAPI, BlueprintType)
class UPawnInitializationConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/// Components managed by the pawn initialization component.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Initialization")
	TArray<FPawnInitializationEntry> Components;
};
