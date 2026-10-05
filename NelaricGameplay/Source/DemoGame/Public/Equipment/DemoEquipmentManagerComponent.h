// Copyright (c) 2026 Nelaric Contributors

/** @file DemoEquipmentManagerComponent.h
 * Declares a DA-created pawn equipment manager with authority operations.
 */

#pragma once

#include "Pawn/PawnInitStateComponent.h"
#include "Equipment/DemoEquipmentTypes.h"

#include "DemoEquipmentManagerComponent.generated.h"

class UDemoEquipmentDefinition;
class UDemoEquipmentInstance;
class UDemoEquipmentLoadout;
class UDemoWeaponInstance;

/** @brief Manages pawn-owned equipment with one active item at a time.
 * @details Enable replication in the pawn initialization DA.
 * Operations run on the game thread and require
 * authority.
 * Clients rebuild local instances from replicated snapshots.
 * Equipment remains with the pawn across
 * controller changes.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Demo), meta = (BlueprintSpawnableComponent))
class UDemoEquipmentManagerComponent : public UPawnInitStateComponent
{
	GENERATED_BODY()

public:
	/** @brief Equips a definition into its vacant slot without activating it.
	 * @param Definition Shared asset with a
	 * valid instance class and slot.
	 * @param[out] EquipmentId New identity, or an invalid GUID on failure.
	 *
	 * @return Success or a typed failure; failure leaves equipment unchanged.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Equipment")
	EDemoEquipmentResult Equip(UDemoEquipmentDefinition* Definition, FGuid& EquipmentId);

	/** @brief Removes one item, first deactivating it when necessary.
	 * @param EquipmentId Identity currently
	 * installed on this pawn.
	 * @return Success or a typed failure with no mutation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Equipment")
	EDemoEquipmentResult Unequip(FGuid EquipmentId);

	/** @brief Switches the active item, preserving selection on failure.
	 * @param EquipmentId Identity of an
	 * equipped item; repeating it is safe.
	 * @return Success, or an authority, readiness, lookup or animation
	 * error.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Equipment")
	EDemoEquipmentResult ActivateEquipment(FGuid EquipmentId);

	/// Deactivates the current item; idempotent, authority and game thread only.
	UFUNCTION(BlueprintCallable, Category = "Demo|Equipment")
	EDemoEquipmentResult DeactivateEquipment();

	/// Returns a non-owning local instance, or null; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment")
	UDemoEquipmentInstance* FindEquipment(FGuid EquipmentId) const;

	/// Returns the local item in a slot, or null; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment")
	UDemoEquipmentInstance* FindEquipmentInSlot(FName Slot) const;

	/// Returns the active local instance, or null; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment")
	UDemoEquipmentInstance* GetActiveEquipment() const;

	/// Returns the active local weapon, or null; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Weapon")
	UDemoWeaponInstance* GetActiveWeapon() const;

	/// Stops every item's actions, retaining ammo; authority game thread only.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Weapon")
	void CancelWeaponActions();

	/// Refills every weapon from its definition; authority game thread only.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Weapon")
	void ResetWeaponAmmunition();

	/** @brief Observes an accepted shot after native local presentation.
	 * @details Rendering worlds only; game
	 * thread. Unreliable cosmetic events
	 * never determine damage or ammunition. Weapon effects run through cues;
	 * use this observation hook for hit
	 * markers or other feedback.
	 *
	 * @param Shot Authority collision result and firing equipment identity.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Demo|Weapon")
	void OnWeaponShot(const FDemoWeaponShot& Shot);

	/// Returns local instances; the manager retains ownership until removal.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment")
	TArray<UDemoEquipmentInstance*> GetEquipment() const;

	/** @brief Borrows authority equipment and ammunition change notifications.
	 * @details Game thread only, after the snapshot is committed. Remove owned
	 * bindings before the listener ends. Mutation callbacks may report Busy;
	 * defer gameplay operations until the current equipment operation returns.
	 * @return Manager-owned native delegate; no ownership transfer.
	 */
	Nelaric::DemoEquipment::FStateChanged& OnStateChanged();

	/// Server-only startup loadout; assign in the component Blueprint defaults.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Equipment")
	TObjectPtr<UDemoEquipmentLoadout> InitialLoadout;

public:
	UDemoEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	void RefreshEquipmentPresentation();

protected:
	/// Stops weapon execution when the initialization attempt is revoked.
	virtual void CancelInitGenerationWork() override;

private:
	friend class UDemoWeaponInstance;
	void NotifyWeaponStateChanged();
	void DispatchWeaponShot(const FDemoWeaponShot& Shot);
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastWeaponShot(const FDemoWeaponShot& Shot);
	EDemoEquipmentResult CheckMutation() const;
	bool IsDefinitionValid(const UDemoEquipmentDefinition* Definition) const;
	void PublishState();
	void ApplyInitialLoadout();
	void Shutdown();
	void ClearInstances();
	bool ApplySnapshot();
	UFUNCTION()
	void OnRep_Snapshot();
	UPROPERTY(Transient)
	TArray<TObjectPtr<UDemoEquipmentInstance>> Instances;
	UPROPERTY(ReplicatedUsing = OnRep_Snapshot)
	FDemoEquipmentSnapshot Snapshot;
	FGuid ActiveEquipmentId;
	uint32 LastAppliedRevision = 0;
	bool bStarted = false;
	bool bEnding = false;
	bool bMutating = false;
	bool bInitialLoadoutApplied = false;
	bool bWeaponStateDirty = false;
	Nelaric::DemoEquipment::FStateChanged StateChanged;
};
