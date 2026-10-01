// Copyright (c) 2026 Nelaric Contributors

/** @file GasStateProfile.h Declares ownership and control grant data. */
#pragma once
#include "AttributeSet.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Templates/SubclassOf.h"
#include "GasStateProfile.generated.h"
class UGameplayAbility;
class UGameplayEffect;

/// Logical lifetime of state stored in a participant's ASC.
UENUM(BlueprintType)
enum class EGasStateOwnership : uint8
{
	/// Remains with the participant when avatars change.
	Participant,
	/// Follows the pawn to its next participant ASC.
	Pawn,
	/// Removed when the control relationship ends.
	Control,
};

/// One pawn attribute slot and its initial base value.
USTRUCT(BlueprintType)
struct FGasAttributeRule
{
	GENERATED_BODY()
	/// Attribute whose logical owner is declared here.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	FGameplayAttribute Attribute;
	/// Owner of this attribute across control changes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	EGasStateOwnership Ownership = EGasStateOwnership::Pawn;
	/// Base value used only when creating a new pawn's state.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	float InitialBase = 0.0f;
};

/// Ability granted while a pawn is bound to an ASC.
USTRUCT(BlueprintType)
struct FGasAbilityGrant
{
	GENERATED_BODY()
	/// Ability definition, not a runtime ability instance.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	TSubclassOf<UGameplayAbility> Ability;
	/// Optional action tag consumed by both human and bot input.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	FGameplayTag ActionTag;
	/// Positive ability level.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS", meta = (ClampMin = "1"))
	int32 Level = 1;
};

/** @brief Defines pawn attribute ownership and control grants.
 * @details Keep this asset unchanged while a pawn exists. Attribute rules
 * transfer base values; persistent modifiers transfer as effects. The
 * participant provides attribute set classes. All access is game-thread.
 */
UCLASS(MinimalAPI, BlueprintType)
class UGasStateProfile : public UDataAsset
{
	GENERATED_BODY()
public:
	/** @brief Validates unique attribute slots and concrete ability classes.
	 * @details Read-only on the game thread; does not create gameplay state.
	 * @return Whether this profile can be used for control transfers.
	 */
	GAMEPLAYABILITIESINTEGRATION_API bool IsValidProfile() const;
	/// Attribute ownership; omitted attributes remain participant-owned.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	TArray<FGasAttributeRule> Attributes;
	/// Pawn abilities removed and reconstructed on avatar changes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	TArray<FGasAbilityGrant> Abilities;
	/// Non-instant, unstacked effects for this control relationship only.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayEffect>> ControlEffects;
	/// Extra control effects when a human participant operates this pawn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayEffect>> PlayerControlEffects;
	/// Extra control effects when a bot participant operates this pawn.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	TArray<TSubclassOf<UGameplayEffect>> BotControlEffects;

public:
};
