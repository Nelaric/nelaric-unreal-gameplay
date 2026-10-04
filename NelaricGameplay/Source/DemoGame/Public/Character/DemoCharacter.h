// Copyright (c) 2026 Nelaric Contributors

/** @file DemoCharacter.h
 * Declares the base character for DemoGame.
 */

#pragma once

#include "NelaricGasCharacter.h"
#include "ObjectPool/CharacterPoolHelper.h"
#include "ObjectPool/PoolableCharacter.h"
#include "Templates/SubclassOf.h"

#include "DemoCharacter.generated.h"

class UDemoAnimationDataInstance;
class UGasStateProfile;
class UPawnControlComponent;
class UPawnInitializationComponent;

namespace Nelaric::UnitAnimation
{
class IAnimationDataUpdater;
}

/** @brief Base character for game-specific demo characters.
 * @details Keeps the GAS base and implements pooling through an interface.
 * @note The world owns instances; access on the game thread.
 * @note Ordinary spawns start active; deferred pool spawns start idle.
 * @note Shared transitions leave GAS state with its existing owner.
 * @note Configure the main animation through
 * AnimationDataClass.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoCharacter : public ANelaricGasCharacter, public Nelaric::ObjectPool::IPoolableCharacter
{
	GENERATED_BODY()

public:
	/** @brief Constructs a demo character with inherited pawn initialization.
	 * @details Called by Unreal on the game thread when spawning the actor.
	 * @param ObjectInitializer Initializer for inherited default subobjects.
	 */
	DEMOGAME_API ADemoCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** @brief Returns the main instance's native updater reference.
	 * @details Game thread only; the mesh owns the
	 * instance.
	 * @pre The main animation instance exists.
	 * @note The scheduler protects worker access.
	 *
	 * @note An unexpected class causes a fatal error before the downcast.
	 * @return Interface of the current main
	 * animation instance.
	 */
	DEMOGAME_API Nelaric::UnitAnimation::IAnimationDataUpdater& GetAnimationDataUpdater() const;

	/// Returns committed local health, or zero without an ASC; game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Combat")
	float GetHealth() const;

	/// Returns committed local maximum health, or zero; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Combat")
	float GetMaxHealth() const;

	/// Returns whether committed health is positive; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Combat")
	bool IsAlive() const;

	/// Cancels active abilities and weapon actions; authority game thread.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Combat")
	void CancelCombatActions();

	/** @brief Restores maximum health and authored weapon ammunition.
	 * @details Authority game thread only. Call for
	 * a new life, outside
	 * control transfers. Pool activation preserves combat state by default.
	 * @return False
	 * without committed GAS readiness; otherwise true.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Combat")
	bool ResetCombatState();

	/** @brief Observes health after a local effect or replicated change.
	 * @details Game thread only. Query getters
	 * when attaching a new UI;
	 * control transfers may update the binding without an effect callback.
	 *
	 * @param Health Current health after clamping.
	 * @param MaxHealth Current health limit.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Combat")
	void OnHealthChanged(float Health, float MaxHealth);

	/** @brief Observes death once per life after combat actions are stopped.
	 * @details Runs locally on rendering
	 * worlds and on authority. Blueprint
	 * gameplay mutations require authority. ResetCombatState starts a new
	 * life.
	 * @param DamageInstigator Source actor; may be null on replicas.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Combat")
	void OnDeath(AActor* DamageInstigator);

	/** @brief Main animation class implementing the native updater.
	 * @details Set character defaults before
	 * spawning.
	 * @note Native subclasses implement UpdateAnimationData.
	 * @note Animation blueprints derive
	 * from these native subclasses.
	 * @note Initialization applies this typed class to the mesh.
	 * @note Null
	 * leaves the mesh without a main animation instance.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Animation")
	TSubclassOf<UDemoAnimationDataInstance> AnimationDataClass;

	/** @brief Restores native walking and presentation for an inactive lease.
	 * @details Call on authority after BeginPlay and committed GAS readiness.
	 * @note Collision callbacks must
	 * preserve the actor and pool.
	 * @note Does not reset retained GAS state.
	 * @note Restores captured control settings and starts existing Ready bots.
	 * @param Transform World placement chosen by the caller without a sweep.
	 * @return False while GAS is unready or native activation fails.
	 */
	DEMOGAME_API bool ActivateFromPool(const FTransform& Transform) override;

	/** @brief Stops native movement and hides this character between leases.
	 * @details Game thread only; safe to repeat.
	 * @note Disables control requests and stops pawn and AI controller brains.
	 * @note Gameplay resolves player possession before returning a lease.
	 * @note Retains GAS state and controller associations.
	 */
	DEMOGAME_API void DeactivateToPool() override;

	/// Returns the native active state on the game thread.
	inline bool IsPoolActive() const override
	{
		check(IsInGameThread());
		return PoolState.bActive;
	}

public:
	void NotifyCombatHealthChanged(AActor* DamageInstigator = nullptr);
	DEMOGAME_API virtual void PrepareForPool() override;
	DEMOGAME_API virtual void PostInitializeComponents() override;
	DEMOGAME_API virtual void PostNetInit() override;
	DEMOGAME_API virtual void SetActorHiddenInGame(bool bNewHidden) override;
	DEMOGAME_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void CancelLocalWeaponActions();
	void InitializePoolControlPolicy();
	bool HasConfiguredPoolControlPolicy() const;
	UFUNCTION()
	void HandlePoolPawnInitialized(UPawnInitializationComponent* Initialization);
	void ApplyPoolControlPolicy();
	void StopPoolBotLogic();

	Nelaric::ObjectPool::FCharacterPoolState PoolState{true};
	bool bPoolComponentsInitialized = false;
	bool bPoolInitializationSubscribed = false;
	bool bPoolControlPolicyActive = false;
	bool bPoolAllowPlayerControl = false;
	bool bPoolAllowReturnControl = false;
	bool bPoolReturnToBot = false;
	bool bPoolStartBotLogicOnReady = false;
	bool bDeathHandled = false;
	UPROPERTY(ReplicatedUsing = OnRep_CombatDead)
	bool bCombatDead = false;
	UFUNCTION()
	void OnRep_CombatDead();

	UPROPERTY(Transient)
	TObjectPtr<UPawnControlComponent> PoolControlPolicy;

	UPROPERTY(VisibleAnywhere, Category = "Nelaric|GAS")
	TObjectPtr<UGasStateProfile> DefaultStateProfile;
};
