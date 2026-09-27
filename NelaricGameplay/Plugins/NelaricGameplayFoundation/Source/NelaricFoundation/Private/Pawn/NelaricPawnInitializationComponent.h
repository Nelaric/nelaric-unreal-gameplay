// Copyright (c) 2026 Nelaric

/** @file NelaricPawnInitializationComponent.h
 * Declares a one-time initialization gate for pawn gameplay components.
 */

#pragma once

#include "Pawn/NelaricPawnComponent.h"

#include "NelaricPawnInitializationComponent.generated.h"

class UNelaricPawnInitializationComponent;

/// Announces that a pawn has passed its initialization gate.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNelaricPawnInitialized, UNelaricPawnInitializationComponent*, Component);

/** @brief Coordinates one-time initialization for an owning pawn.
 *
 * @details Add this component to a pawn and override CanInitializePawn for
 * game-specific readiness. The component tries once at BeginPlay; call
 * TryInitializePawn again when a required dependency becomes available.
 * Initialization is local to this component and is not replicated. The pawn
 * owns the component. Operations and notifications run on the game thread.
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
};
