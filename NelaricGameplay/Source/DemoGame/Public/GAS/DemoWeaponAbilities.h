// Copyright (c) 2026 Nelaric Contributors

/** @file DemoWeaponAbilities.h Declares shared fire and reload execution. */
#pragma once

#include "NelaricGameplayAbility.h"
#include "Equipment/DemoWeaponTypes.h"
#include "TimerManager.h"
#include "DemoWeaponAbilities.generated.h"

class UDemoWeaponInstance;

/** @brief Adapts Action.Fire to authority shots on the selected weapon.
 * @details Local prediction installs release
 * handling only. Damage, ammo
 * and repeated shots execute on authority. Control changes cancel execution.
 */
UCLASS(MinimalAPI)
class UDemoFireAbility : public UNelaricGameplayAbility
{
	GENERATED_BODY()
public:
public:
	UDemoFireAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                                const FGameplayTagContainer* SourceTags = nullptr,
	                                const FGameplayTagContainer* TargetTags = nullptr,
	                                FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                        FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
	                        bool bWasCancelled) override;

private:
	void FireNextShot();
	void HandleWeaponCanceled();
	UFUNCTION()
	void HandleInputReleased(float TimeHeld);
	TWeakObjectPtr<UDemoWeaponInstance> Weapon;
	FTimerHandle FireTimer;
	FDelegateHandle CanceledHandle;
};

/** @brief Adapts Action.Reload to one cancelable authority reload.
 * @details Ammo stays on the item across control
 * changes. The ability ends
 * when its matching reload finishes or when normal cancellation runs.
 */
UCLASS(MinimalAPI)
class UDemoReloadAbility : public UNelaricGameplayAbility
{
	GENERATED_BODY()
public:
public:
	UDemoReloadAbility();
	virtual bool CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                                const FGameplayTagContainer* SourceTags = nullptr,
	                                const FGameplayTagContainer* TargetTags = nullptr,
	                                FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                             FGameplayAbilityActivationInfo ActivationInfo,
	                             const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	                        FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility,
	                        bool bWasCancelled) override;

private:
	void HandleReloadFinished(FGuid FinishedId, EDemoWeaponResult Result);
	void HandleWeaponCanceled();
	TWeakObjectPtr<UDemoWeaponInstance> Weapon;
	FGuid ReloadId;
	FDelegateHandle FinishedHandle;
	FDelegateHandle CanceledHandle;
};
