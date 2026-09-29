// Copyright (c) 2026 Nelaric
// Copyright Epic Games, Inc. All Rights Reserved.

/** @file NelaricInputConfig.h
 * Defines native input actions and mapping contexts, adapted from Lyra.
 */
#pragma once
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "NelaricInputConfig.generated.h"
class UInputAction;
class UInputMappingContext;

/// Associates a native input action with an exact gameplay tag.
USTRUCT(BlueprintType)
struct FNelaricInputAction
{
	GENERATED_BODY()
	/// Asset retained by the configuration; null entries are skipped.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;
	/// Identifier for manual binding, independent of any ability system.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

/// Describes an active mapping context and optional remapping registration.
USTRUCT(BlueprintType)
struct FNelaricInputMapping
{
	GENERATED_BODY()
	/// Loaded context retained by the configuration; null entries are skipped.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;
	/// Higher priorities win conflicting mappings.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	int32 Priority = 0;
	/// Registers player mappable keys with Enhanced Input user settings.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	bool bRegisterWithSettings = true;
};

/** @brief Immutable configuration for native input on a local pawn.
 * @details Create a data asset and assign actions and mapping contexts.
 * The asset owns its references. Read on the game thread.
 */
UCLASS(MinimalAPI, BlueprintType, Const)
class UNelaricInputConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	/** @brief Finds the first non-null action matching an exact valid tag.
	 * @details Call on the game thread. Invalid or absent tags return null.
	 * @param InputTag Exact native action identifier.
	 * @return Non-owning action retained by this asset, or null.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Input")
	GAMEPLAYRUNTIME_API const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag) const;
	/// Manually bound actions; duplicate tags resolve to the first valid action.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputTag"))
	TArray<FNelaricInputAction> NativeInputActions;
	/// Contexts activated while the owning pawn has local input.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "MappingContext"))
	TArray<FNelaricInputMapping> MappingContexts;
};
