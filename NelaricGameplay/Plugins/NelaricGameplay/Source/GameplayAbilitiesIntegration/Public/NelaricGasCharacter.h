// Copyright (c) 2026 Nelaric Contributors

/** @file NelaricGasCharacter.h Declares a ready-to-use GAS character. */
#pragma once
#include "Pawn/NelaricCharacter.h"
#include "AbilitySystemInterface.h"
#include "NelaricGasCharacter.generated.h"
class UPawnGasBindingComponent;

/// Character binding PlayerState GAS; control policy is configured separately.
UCLASS(MinimalAPI, Blueprintable)
class ANelaricGasCharacter : public ANelaricCharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()
public:
	/** @brief Returns the currently associated participant or custody ASC.
	 * @details Borrowed on the game thread. Null until references resolve.
	 * @return ASC stored on the current state owner's PlayerState.
	 */
	GAMEPLAYABILITIESINTEGRATION_API virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	/** @brief Returns the character-owned GAS lifecycle component.
	 * @details Borrowed on the game thread during this actor's lifetime.
	 * @return Native binding and transfer component.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|GAS")
	GAMEPLAYABILITIESINTEGRATION_API UPawnGasBindingComponent* GetGasBinding() const;

public:
	GAMEPLAYABILITIESINTEGRATION_API
	ANelaricGasCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

private:
	UPROPERTY(VisibleAnywhere, Category = "Nelaric|GAS")
	TObjectPtr<UPawnGasBindingComponent> GasBinding;
};
