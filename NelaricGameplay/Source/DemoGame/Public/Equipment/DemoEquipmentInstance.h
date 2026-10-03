// Copyright (c) 2026 Nelaric Contributors

/** @file DemoEquipmentInstance.h
 * Declares local equipment instances and their weapon specialization.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "DemoEquipmentInstance.generated.h"

class AActor;
class APawn;
class UDemoEquipmentDefinition;
class UDemoEquipmentManagerComponent;
class UDemoPawnAnimationLayerComponent;
class UDemoWeaponDefinition;

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

private:
	friend class UDemoEquipmentInstance;
	friend class UDemoEquipmentManagerComponent;
	bool CanActivate() const;
	void RefreshAnimationLayer();
	void ReleaseAnimationLayer();
	TWeakObjectPtr<UDemoPawnAnimationLayerComponent> AnimationProvider;
	FGuid AnimationHandle;
};
