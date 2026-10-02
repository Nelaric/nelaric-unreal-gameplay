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

	/// Returns local instances; the manager retains ownership until removal.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment")
	TArray<UDemoEquipmentInstance*> GetEquipment() const;

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

private:
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
};
