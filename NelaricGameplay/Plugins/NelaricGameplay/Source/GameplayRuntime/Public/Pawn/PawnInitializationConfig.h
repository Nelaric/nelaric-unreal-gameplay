// Copyright (c) 2026 Nelaric

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
 * Authority and clients independently create selected entries under the
 * same ID. Configured dynamic instances, Init State, and Generation remain
 * local; replicate gameplay data through separate UE paths. A component
 * requiring dynamic instance replication needs a separate creation path.
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

	/// Create this component on non-authority peers.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Initialization")
	bool bCreateOnClient = true;

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
