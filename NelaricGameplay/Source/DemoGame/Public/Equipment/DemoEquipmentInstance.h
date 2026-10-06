// Copyright (c) 2026 Nelaric Contributors

/** @file DemoEquipmentInstance.h
 * Declares local equipment instances and their weapon specialization.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Equipment/DemoWeaponTypes.h"
#include "TimerManager.h"

#include "DemoEquipmentInstance.generated.h"

class AActor;
class AController;
class APawn;
class UAnimMontage;
class UDemoEquipmentDefinition;
class UDemoEquipmentManagerComponent;
class UDemoPawnAnimationLayerComponent;
class UDemoWeaponDefinition;
class UNelaricAbilitySystemComponent;
class USkeletalMeshComponent;

/** @brief Local runtime identity and lifecycle of one equipped item.
 * @details The manager owns the instance through
 * GC references. Instances
 * are reconstructed from replicated state on clients, not replicated as
 * subobjects. All
 * operations and Blueprint callbacks run on the game thread.
 * Retained instances become detached after unequip; query
 * before using them.
 */
UCLASS(MinimalAPI, BlueprintType, Blueprintable)
class UDemoEquipmentInstance : public UObject
{
	GENERATED_BODY()

public:
	/// Returns the stable equipment identity; call on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment")
	FGuid GetEquipmentId() const;

	/// Returns the shared definition, or null after removal; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment")
	UDemoEquipmentDefinition* GetDefinition() const;

	/// Returns the non-owning pawn, or null after removal; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment")
	APawn* GetPawn() const;

	/// Returns whether this local instance is active; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment")
	bool IsActive() const;

	/// Returns local cosmetic actors, which may still be pending their mesh.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment")
	TArray<AActor*> GetVisualActors() const;

	/** @brief Observes installation of this local instance.
	 * @details Runs on each machine. Check pawn authority
	 * for gameplay logic.
	 * Reentrant manager mutations return Busy. Visuals may still be pending.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Equipment")
	void OnEquipped();

	/// Observes removal after deactivation, before references are cleared.
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Equipment")
	void OnUnequipped();

	/// Observes activation after the weapon requests its animation layer.
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Equipment")
	void OnActivated();

	/// Observes deactivation after the weapon releases its animation request.
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Equipment")
	void OnDeactivated();

public:
	virtual UWorld* GetWorld() const override;

private:
	friend class UDemoEquipmentManagerComponent;
	void Initialize(APawn* InPawn, UDemoEquipmentDefinition* InDefinition, FGuid InId);
	void SetActive(bool bNewActive);
	void Remove();
	void RefreshVisuals();
	void DestroyVisuals();

	UPROPERTY(Transient)
	TObjectPtr<UDemoEquipmentDefinition> Definition;
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> VisualActors;
	TWeakObjectPtr<APawn> Pawn;
	FGuid EquipmentId;
	bool bActive = false;
	bool bRemoved = false;
};

/** @brief Equipment instance that changes its pawn's active animation layer.
 * @details The request belongs to this
 * instance and is revoked on
 * deactivation, removal, or provider replacement. No mesh is assumed on
 * APawn. The
 * pawn must configure an animation provider on rendering worlds.
 */
UCLASS(MinimalAPI, BlueprintType, Blueprintable)
class UDemoWeaponInstance : public UDemoEquipmentInstance
{
	GENERATED_BODY()

public:
	/// Returns the weapon definition, or null after removal; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Weapon")
	UDemoWeaponDefinition* GetWeaponDefinition() const;

	/// Returns the local authority snapshot of ammo and reload; game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Weapon")
	FDemoWeaponState GetWeaponState() const;

	/** @brief Returns elapsed seconds since the last accepted shot.
	 * @details Game thread only. Uses synchronized server world time on clients.
	 * @return Non-negative elapsed seconds, or -1 before firing or when unready.
	 */
	UFUNCTION(BlueprintPure, Category = "Demo|Weapon")
	float GetTimeSinceFiredWeapon() const;

	/// Returns remaining reload seconds using server world time; game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Weapon")
	float GetReloadRemainingTime() const;

	/** @brief Commits one authority hitscan shot from the pawn's view.
	 * @details Game thread only. A miss
	 * consumes one round. Rejected shots
	 * leave ammunition unchanged. Presentation is dispatched by the manager.
	 *
	 * @return Success or a typed readiness, state or rate-limit error.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Weapon")
	EDemoWeaponResult TryFire();

	/** @brief Starts one authority reload without moving ammunition yet.
	 * @details Game thread only.
	 * OnReloadFinished emits one terminal result
	 * for each accepted identity. Canceling leaves both ammo counts
	 * unchanged.
	 * @param[out] ReloadId Cancellation identity, invalid on failure.
	 * @return Success when
	 * scheduled, or a typed failure with no mutation.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Weapon")
	EDemoWeaponResult BeginReload(FGuid& ReloadId);

	/** @brief Cancels only the accepted reload matching an identity.
	 * @details Authority and game thread only. A
	 * stale identity is harmless.
	 * @param ReloadId Identity returned by BeginReload.
	 * @return Success on
	 * cancellation, or NotFound for a stale identity.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Weapon")
	EDemoWeaponResult CancelReload(FGuid ReloadId);

	/// Stops local execution; authority also cancels reload. Game thread only.
	UFUNCTION(BlueprintCallable, Category = "Demo|Weapon")
	void CancelWeaponActions();

	/// Refills authored ammunition and cancels actions; authority game thread.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Weapon")
	void ResetAmmunition();

	/// Borrows game-thread action cancellation notifications for this item.
	Nelaric::DemoEquipment::FWeaponActionsCanceled& OnActionsCanceled();

	/// Borrows game-thread terminal notifications for accepted reloads.
	Nelaric::DemoEquipment::FWeaponReloadFinished& OnReloadFinished();

	/** @brief Observes local ammo or reload changes after state is applied.
	 * @details Runs on each machine's game
	 * thread. Use for UI and custom
	 * presentation; authority checks are required for gameplay mutations.
	 *
	 * @param Previous Previous local snapshot.
	 * @param Current Newly applied local snapshot.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Weapon")
	void OnWeaponStateChanged(const FDemoWeaponState& Previous, const FDemoWeaponState& Current);

public:
	void PresentShot(const FDemoWeaponShot& Shot, const FTransform& Muzzle);
	void ValidatePendingActions();

private:
	friend class UDemoEquipmentInstance;
	friend class UDemoEquipmentManagerComponent;
	bool CanActivate() const;
	void RefreshAnimationLayer();
	void ReleaseAnimationLayer();
	void InitializeWeaponState();
	void ApplyWeaponState(const FDemoWeaponState& NewState);
	void PublishWeaponState(const FDemoWeaponState& Previous);
	void FinishReload(EDemoWeaponResult Result);
	EDemoWeaponResult CheckWeaponCommand() const;
	UDemoEquipmentManagerComponent* GetManager() const;
	void RefreshReloadPresentation();
	FDemoWeaponState WeaponState;
	FTimerHandle ReloadTimer;
	FGuid PendingReloadId;
	TWeakObjectPtr<AController> ReloadController;
	TWeakObjectPtr<UNelaricAbilitySystemComponent> ReloadAbilitySystem;
	TWeakObjectPtr<USkeletalMeshComponent> ReloadPresentationMesh;
	TWeakObjectPtr<UAnimMontage> PlayingReloadMontage;
	double NextAllowedFireTime = 0.0;
	bool bWeaponMutating = false;
	bool bCancelingActions = false;
	Nelaric::DemoEquipment::FWeaponActionsCanceled ActionsCanceled;
	Nelaric::DemoEquipment::FWeaponReloadFinished ReloadFinished;
	TWeakObjectPtr<UDemoPawnAnimationLayerComponent> AnimationProvider;
	FGuid AnimationHandle;
};
