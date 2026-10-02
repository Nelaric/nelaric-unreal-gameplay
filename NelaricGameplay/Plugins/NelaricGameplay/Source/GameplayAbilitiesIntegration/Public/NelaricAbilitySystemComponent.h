// Copyright (c) 2026 Nelaric Contributors

/** @file NelaricAbilitySystemComponent.h Declares shared action input. */
#pragma once
#include "AbilitySystemComponent.h"
#include "GasStateProfile.h"
#include "NelaricAbilitySystemComponent.generated.h"
class APawn;
namespace Nelaric::GAS
{
struct FEffectState;
}

/** @brief Participant-owned ASC shared by human and bot avatars.
 * @details State ownership is independent of ASC storage. Input requires
 * a live committed avatar. Transfer mutation blocks input and activation.
 */
UCLASS(MinimalAPI, Blueprintable)
class UNelaricAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
public:
	/** @brief Routes an action press or release to matching ability specs.
	 * @details Call on the owning client's or authority's game thread.
	 * Specs use dynamic source tags. Returns false while unbound or blocked.
	 * @param ActionTag Exact action shared by human and bot adapters.
	 * @param bPressed True for press; false for release or cancellation.
	 * @return Whether a matching ability accepted this input event.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|GAS")
	GAMEPLAYABILITIESINTEGRATION_API bool SubmitAction(FGameplayTag ActionTag, bool bPressed);
	/** @brief Declares the logical owner of an active gameplay effect.
	 * @details Call on the authority game thread immediately after applying
	 * a pawn or control effect. Untracked effects remain participant-owned.
	 * @param Handle Live effect on this ASC.
	 * @param Pawn Pawn to which this effect belongs.
	 * @param Ownership Pawn or control lifetime.
	 * @return Whether ownership was recorded.
	 */
	GAMEPLAYABILITIESINTEGRATION_API bool TrackEffect(FActiveGameplayEffectHandle Handle, APawn* Pawn,
	                                                  EGasStateOwnership Ownership);
	/** @brief Reports whether gameplay observers must defer transfer reactions.
	 * @details Read on the game thread during attribute and effect callbacks.
	 * @return True while this ASC participates in an unsettled state change.
	 */
	GAMEPLAYABILITIESINTEGRATION_API bool IsTransferringState() const;
	/** @brief Returns the current human or bot input source tag.
	 * @details Read on the game thread. Custody and unbound ASCs return an
	 * empty tag. Source tags are metadata; actions use the same input API.
	 * @return Input.Source.Player, Input.Source.AI, or an empty tag.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|GAS")
	GAMEPLAYABILITIESINTEGRATION_API FGameplayTag GetInputSourceTag() const;

public:
	GAMEPLAYABILITIESINTEGRATION_API UNelaricAbilitySystemComponent(const FObjectInitializer& ObjectInitializer);
	GAMEPLAYABILITIESINTEGRATION_API virtual void InitAbilityActorInfo(AActor* InOwnerActor,
	                                                                   AActor* InAvatarActor) override;
	void SetStateTransferBlocked(bool bBlocked);
	void ClearActionInput();
	bool ExportPawnEffects(APawn* Pawn, TArray<Nelaric::GAS::FEffectState>& OutEffects,
	                       const TArray<FActiveGameplayEffectHandle>& CustomEffects = {}) const;
	bool CanReceiveEffects(const TArray<Nelaric::GAS::FEffectState>& Effects) const;
	bool RemovePawnEffects(APawn* Pawn);
	bool RestorePawnEffects(APawn* Pawn, const TArray<Nelaric::GAS::FEffectState>& Effects);

private:
	struct FOwnedEffect
	{
		TWeakObjectPtr<APawn> Pawn;
		EGasStateOwnership Ownership = EGasStateOwnership::Pawn;
	};
	TMap<FActiveGameplayEffectHandle, FOwnedEffect> OwnedEffects;
	bool bStateTransferBlocked = false;
	FGameplayTag BoundInputSourceTag;
	void HandleEffectRemoved(const FActiveGameplayEffect& Effect);
};
