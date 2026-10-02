// Copyright (c) 2026 Nelaric Contributors

/** @file DemoPawnAnimationLayerComponent.h
 * Declares a local pawn provider for source-owned weapon animation layers.

 */

#pragma once

#include "Pawn/PawnInitStateComponent.h"

#include "DemoPawnAnimationLayerComponent.generated.h"

class UAnimInstance;
class UDemoEquipmentInstance;
class USkeletalMeshComponent;

namespace Nelaric::DemoEquipment
{
/// Local non-owning request; sequence order resolves the single weapon slot.
struct FAnimationRequest
{
	/// Equipped source; weak ownership allows automatic removal after GC.
	TWeakObjectPtr<UDemoEquipmentInstance> Source;
	/// Concrete layer class retained by the source definition.
	TWeakObjectPtr<UClass> LayerClass;
	/// Acquisition order; a larger value wins the weapon channel.
	uint64 Sequence = 0;
};
} // namespace Nelaric::DemoEquipment

/** @brief Applies the newest live weapon request to a configured pawn mesh.
 * @details Configure a Blueprint subclass
 * through the pawn initialization
 * asset. Requests and linking are local and game-thread-only. Tick detects
 * mesh
 * or anim instance replacement and prunes dead sources. Dedicated
 * servers skip presentation. The pawn owns this
 * component and its requests.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Demo), meta = (BlueprintSpawnableComponent))
class UDemoPawnAnimationLayerComponent : public UPawnInitStateComponent
{
	GENERATED_BODY()

public:
	/** @brief Acquires a source-owned request without requiring a ready mesh.
	 * @param Source Equipped instance
	 * belonging to this pawn.
	 * @param LayerClass Concrete weapon layer blueprint class.
	 * @return Valid handle,
	 * or an invalid GUID for invalid configuration.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Equipment|Animation")
	FGuid AcquireLayer(UDemoEquipmentInstance* Source, TSubclassOf<UAnimInstance> LayerClass);

	/** @brief Releases only the matching request and reapplies the winner.
	 * @param Handle Handle returned by this
	 * provider; stale handles are safe.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Equipment|Animation")
	void ReleaseLayer(FGuid Handle);

	/// Reports whether this provider still owns a handle; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Equipment|Animation")
	bool HasLayerRequest(FGuid Handle) const;

	/** @brief Checks a concrete layer and configured target on the game thread.
	 * @details An unavailable mesh can
	 * be prepared later. An existing mesh
	 * rejects a layer authored for a different skeleton.
	 */
	bool CanUseLayer(TSubclassOf<UAnimInstance> LayerClass) const;

	/// Exact pawn component object name; None is invalid.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Equipment|Animation")
	FName MeshComponentName;

	/// Optional explicit unarmed layer; null uses the main blueprint defaults.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Equipment|Animation")
	TSubclassOf<UAnimInstance> DefaultAnimationLayer;

public:
	UDemoPawnAnimationLayerComponent(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

private:
	USkeletalMeshComponent* ResolveMesh() const;
	void RefreshLayer();
	void ClearAppliedLayer();
	TMap<FGuid, Nelaric::DemoEquipment::FAnimationRequest> Requests;
	TWeakObjectPtr<USkeletalMeshComponent> AppliedMesh;
	TWeakObjectPtr<UAnimInstance> AppliedAnimInstance;
	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> AppliedClass;
	uint64 NextSequence = 0;
	bool bAppliedLinkWasPresent = false;
	bool bEnding = false;
};
