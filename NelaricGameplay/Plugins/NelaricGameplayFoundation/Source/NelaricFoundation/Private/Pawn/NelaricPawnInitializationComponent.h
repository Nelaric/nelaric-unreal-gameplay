// Copyright (c) 2026 Nelaric

/** @file NelaricPawnInitializationComponent.h
 * Declares a one-time initialization gate for pawn gameplay components.
 */

#pragma once

#include "Pawn/NelaricPawnComponent.h"
#include "Containers/Map.h"
#include "UObject/WeakObjectPtr.h"

#include "NelaricPawnInitializationComponent.generated.h"

class UNelaricPawnInitializationComponent;
class UNelaricPawnInitializationConfig;
class UActorComponent;

/// Announces that a pawn has passed its initialization gate.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNelaricPawnInitialized, UNelaricPawnInitializationComponent*, Component);

/** @brief Coordinates one-time initialization for an owning pawn.
 *
 * @details Add this component to a pawn and override CanInitializePawn for
 * game-specific readiness. Assign InitializationConfig on a pawn or
 * character Blueprint to create components. It tries at BeginPlay; call
 * TryInitializePawn again when required context changes. Initialization is
 * local and is not replicated. The pawn owns the component. Operations and
 * notifications run on the game thread.
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
	 * initialization pending. Successful initialization broadcasts once.
	 * @return Whether initialization has completed, including earlier calls.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Pawn|Initialization")
	bool TryInitializePawn();

	/** @brief Reports whether this component initialized its pawn.
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

	/** @brief Broadcasts once when initialization succeeds on this machine.
	 *
	 * @details Bind before BeginPlay to observe immediate initialization.
	 * Late listeners can query IsPawnInitialized(). Runs on the game thread.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Nelaric|Pawn|Initialization")
	FNelaricPawnInitialized OnPawnInitialized;

	/** @brief Asset that controls local component creation and readiness.
	 * @details Set on a Pawn or Character derived Blueprint. IDs, dependencies,
	 * creation sides, and required flags come only from this asset.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nelaric|Pawn|Initialization")
	TObjectPtr<UNelaricPawnInitializationConfig> InitializationConfig;

public:
	UNelaricPawnInitializationComponent(const FObjectInitializer& ObjectInitializer);
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	virtual bool CanInitializePawn_Implementation() const;

private:
	bool bPawnInitialized = false;
	bool bInitializationInProgress = false;
	bool bInitializationAllowed = false;
	bool bConfigValid = true;
	TMap<FName, TWeakObjectPtr<UActorComponent>> ConfiguredComponents;
	void CreateConfiguredComponents();
	bool AreRequiredComponentsReady() const;
};
