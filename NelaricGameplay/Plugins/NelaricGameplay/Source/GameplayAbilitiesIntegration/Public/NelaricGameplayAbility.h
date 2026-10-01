// Copyright (c) 2026 Nelaric Contributors

/** @file NelaricGameplayAbility.h Declares shared control readiness gating. */
#pragma once
#include "Abilities/GameplayAbility.h"
#include "GasStateProfile.h"
#include "NelaricGameplayAbility.generated.h"

/// Required behavior of an active ability when its avatar is transferred.
UENUM(BlueprintType)
enum class EGasAbilityControlPolicy : uint8
{
	/// Ends through normal cancellation and can be activated again.
	Cancel,
	/// A registered adapter checkpoints and reconstructs execution.
	PreserveExecution,
	/// A registered adapter moves execution to an independent owner.
	DetachExecution,
	/// A registered adapter settles committed work before releasing state.
	FinishExecution,
};

/** @brief Ability base that respects committed avatar state.
 * @details Derive human and bot abilities from this class. Domain transfer
 * extensions preserve custom execution state. Standard cancellation uses
 * the normal ability cleanup path. Calls run on the game thread.
 */
UCLASS(Abstract, MinimalAPI, Blueprintable)
class UNelaricGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	/// Active execution policy; non-cancel policies require an adapter.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nelaric|GAS")
	EGasAbilityControlPolicy ControlChangePolicy = EGasAbilityControlPolicy::Cancel;
	/// Cooldown lifetime; pawn cooldowns migrate with remaining duration.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nelaric|GAS")
	EGasStateOwnership CooldownOwnership = EGasStateOwnership::Pawn;

public:
	GAMEPLAYABILITIESINTEGRATION_API
	UNelaricGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	GAMEPLAYABILITIESINTEGRATION_API virtual bool
	CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                   const FGameplayTagContainer* SourceTags = nullptr,
	                   const FGameplayTagContainer* TargetTags = nullptr,
	                   FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	GAMEPLAYABILITIESINTEGRATION_API virtual void
	ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	              FGameplayAbilityActivationInfo ActivationInfo) const override;
};
