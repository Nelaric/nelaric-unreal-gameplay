// Copyright (c) 2026 Nelaric Contributors

/** @file NelaricGasPlayerState.h Declares the common GAS participant. */
#pragma once
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GasStateProfile.h"
#include "NelaricGasPlayerState.generated.h"
class UNelaricAbilitySystemComponent;

/** @brief Holds the same ASC and participant setup for humans and bots.
 * @details Attribute sets and participant defaults initialize once on
 * authority. Avatar binding does not repeat defaults. World owns this actor.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricGasPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	/** @brief Returns the participant's persistent ASC.
	 * @details Borrowed on the game thread; valid during this actor's lifetime.
	 * @return ASC owned by this player state.
	 */
	GAMEPLAYABILITIESINTEGRATION_API virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	/** @brief Returns the typed ASC used by binding and action adapters.
	 * @details Borrowed on the game thread; not transferred to another actor.
	 * @return This participant's ASC.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|GAS")
	GAMEPLAYABILITIESINTEGRATION_API UNelaricAbilitySystemComponent* GetNelaricAbilitySystem() const;
	/** @brief Distinguishes pawn state custody from a gameplay participant.
	 * @details Read on the game thread when building participant rosters.
	 * @return True for an owner pawn's non-participating state carrier.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|GAS")
	GAMEPLAYABILITIESINTEGRATION_API bool IsStateCustodian() const;
	/// Attribute set classes installed on authority before pawn binding.
	UPROPERTY(EditDefaultsOnly, Category = "Nelaric|GAS")
	TArray<TSubclassOf<UAttributeSet>> AttributeSetClasses;
	/// Participant defaults; only participant-owned attribute rules apply.
	UPROPERTY(EditDefaultsOnly, Category = "Nelaric|GAS")
	TObjectPtr<UGasStateProfile> ParticipantProfile;

public:
	GAMEPLAYABILITIESINTEGRATION_API
	ANelaricGasPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	GAMEPLAYABILITIESINTEGRATION_API virtual void PostInitializeComponents() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Nelaric|GAS")
	TObjectPtr<UNelaricAbilitySystemComponent> AbilitySystem;
};
