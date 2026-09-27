// Copyright (c) 2026 Nelaric

/** @file NelaricPawnInitializationComponent.h
 * Declares a reversible readiness gate for pawn gameplay components.
 */

#pragma once

#include "Pawn/NelaricPawnComponent.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"

#include "NelaricPawnInitializationComponent.generated.h"

class UNelaricPawnInitializationComponent;
class UNelaricPawnInitializationConfig;
class UActorComponent;

/// Announces a transition into pawn Ready.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNelaricPawnInitialized, UNelaricPawnInitializationComponent*, Component);

/// Announces a transition out of pawn Ready.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNelaricPawnInitializationRevoked, UNelaricPawnInitializationComponent*,
                                            Component);

/** @brief Coordinates local, reversible readiness for an owning pawn.
 *
 * @details Add this component to a pawn and override CanInitializePawn for
 * game-specific readiness. Assign InitializationConfig on a pawn or
 * character Blueprint to create components. It tries at BeginPlay; call
 * TryInitializePawn again when required context changes. Initialization is
 * local and is not replicated. Each side creates its own configured
 * components, whose dynamic instances do not replicate. Replicate gameplay
 * data through separate UE paths and retry locally when that data arrives.
 * The pawn owns the component. Operations and notifications run on the game
 * thread.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Nelaric), meta = (BlueprintSpawnableComponent))
class UNelaricPawnInitializationComponent : public UNelaricPawnComponent
{
	GENERATED_BODY()

public:
	/** @brief Tries to initialize after BeginPlay when the pawn is ready.
	 *
	 * @details Call on the game thread when readiness may have changed. A
	 * missing pawn, an ended component, or a failed readiness check leaves
	 * readiness pending. Each transition into Ready broadcasts.
	 * @return Whether the pawn is currently Ready.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Pawn|Initialization")
	bool TryInitializePawn();

	/** @brief Reports whether this pawn is currently Ready.
	 *
	 * @details Call on the game thread. Returns false before BeginPlay and
	 * after EndPlay. This state is not replicated.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Pawn|Initialization")
	bool IsPawnInitialized() const;

	/** @brief Checks whether the pawn's required context is available.
	 *
	 * @details Called on the game thread by TryInitializePawn after BeginPlay.
	 * The default accepts any valid owning pawn. Override in C++ or Blueprint
	 * to require more context; retry when that context changes.
	 * @return Whether initialization may complete now.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Nelaric|Pawn|Initialization")
	bool CanInitializePawn() const;

	/** @brief Replaces the local component configuration as one new round.
	 * @details Call on the game thread. During play, this revokes current Ready,
	 * destroys managed instances, validates the new asset, and tries readiness.
	 * A null asset creates no components and uses only CanInitializePawn.
	 * @param NewConfig Asset to use for the next local initialization round.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Pawn|Initialization")
	void SetInitializationConfig(UNelaricPawnInitializationConfig* NewConfig);

	/** @brief Broadcasts on each transition into Ready on this machine.
	 *
	 * @details Bind before BeginPlay to observe immediate initialization.
	 * Late listeners can query IsPawnInitialized(). Runs on the game thread.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Nelaric|Pawn|Initialization")
	FNelaricPawnInitialized OnPawnInitialized;

	/** @brief Broadcasts when current pawn Ready is revoked.
	 * @details Required participant invalidation, configuration replacement,
	 * and EndPlay revoke Ready on the game thread.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Nelaric|Pawn|Initialization")
	FNelaricPawnInitializationRevoked OnPawnInitializationRevoked;

	/** @brief Asset that controls local component creation and readiness.
	 * @details Set on a Pawn or Character derived Blueprint. IDs, dependencies,
	 * creation sides, and required flags come only from this asset.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nelaric|Pawn|Initialization")
	TObjectPtr<UNelaricPawnInitializationConfig> InitializationConfig;

public:
	UNelaricPawnInitializationComponent(const FObjectInitializer& ObjectInitializer);
	bool IsConfiguredInstance(FName ComponentId, const UActorComponent* Component) const;
	bool HasConfiguredId(FName ComponentId) const;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual bool CanInitializePawn_Implementation() const;

private:
	bool bPawnInitialized = false;
	bool bInitializationInProgress = false;
	bool bInitializationAllowed = false;
	bool bConfigValid = true;
	UPROPERTY(Transient)
	TObjectPtr<UNelaricPawnInitializationConfig> ActiveConfig;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UActorComponent>> ConfiguredComponents;
	TArray<FName> RequiredComponentIds;
	bool bConfiguredComponentsCreated = false;
	bool bInitializationEnded = false;
	FDelegateHandle WorldBeginTearDownHandle;
	void RevokePawnReady();
	void ShutdownInitialization();
	void HandleWorldBeginTearDown(UWorld* World);
	void DestroyConfiguredComponents();
	bool ValidateConfiguration() const;
	void CreateConfiguredComponents();
	bool AreRequiredComponentsReady() const;
};
