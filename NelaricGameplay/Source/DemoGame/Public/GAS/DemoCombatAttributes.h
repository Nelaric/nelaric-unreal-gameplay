// Copyright (c) 2026 Nelaric Contributors

/** @file DemoCombatAttributes.h Declares example pawn-owned attributes. */
#pragma once
#include "AttributeSet.h"
#include "DemoCombatAttributes.generated.h"

/// Example character attributes stored exclusively in PlayerState ASCs.
UCLASS(MinimalAPI)
class UDemoCombatAttributes : public UAttributeSet
{
	GENERATED_BODY()
public:
	/** @brief Returns the current health attribute identity.
	 * @details Use on the game thread for effects, profiles and UI binding.
	 * @return Health property on this attribute set type.
	 */
	DEMOGAME_API static FGameplayAttribute GetHealthAttribute();
	/** @brief Returns the maximum health attribute identity.
	 * @details Use on the game thread for effects and control bonuses.
	 * @return MaxHealth property on this attribute set type.
	 */
	DEMOGAME_API static FGameplayAttribute GetMaxHealthAttribute();
	/** @brief Returns the base attack attribute identity.
	 * @details Use on the game thread for effects and ownership profiles.
	 * @return Attack property on this attribute set type.
	 */
	DEMOGAME_API static FGameplayAttribute GetAttackAttribute();
	/// Current health resource; positive while alive.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Attributes")
	FGameplayAttributeData Health;
	/// Health upper bound, including current effect modifiers.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Attributes")
	FGameplayAttributeData MaxHealth;
	/// Example attack stat whose base follows the pawn.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Attack, Category = "Attributes")
	FGameplayAttributeData Attack;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void OnRep_Attack(const FGameplayAttributeData& OldValue);
};
