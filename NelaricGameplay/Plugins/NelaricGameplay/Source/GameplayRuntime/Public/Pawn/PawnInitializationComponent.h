// Copyright (c) 2026 Nelaric Contributors

/** @file PawnInitializationComponent.h
 * Declares a reversible readiness gate for pawn gameplay components.
 */

#pragma once

#include "Pawn/NelaricPawnComponent.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"

#include "PawnInitializationComponent.generated.h"

class UPawnInitializationComponent;
class UPawnInitializationConfig;
class UActorComponent;

/// Game-thread callback observing local pawn readiness.
DECLARE_DYNAMIC_DELEGATE_OneParam(FPawnInitializationCallback, UPawnInitializationComponent*, Component);

/// Announces a transition into pawn Ready.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPawnInitialized, UPawnInitializationComponent*, Component);

/// Announces a transition out of pawn Ready.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPawnInitializationRevoked, UPawnInitializationComponent*, Component);

/** @brief Coordinates local, reversible readiness for an owning pawn.
 *
 * @details Add this component to a pawn and override CanInitializePawn for
 * game-specific readiness. Assign InitializationConfig on a pawn or
 * character Blueprint to create components. It tries at BeginPlay; call
 * TryInitializePawn to retry the pawn gate. InvalidatePawnContext restarts
 * participant bindings after context replacement. Initialization is
 * local and is not replicated. Each side creates its own configured
 * components, whose dynamic instances do not replicate. Replicate gameplay
 * data through separate UE paths and retry locally when that data arrives.
 * The pawn owns the component. Operations and notifications run on the game
 * thread.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Nelaric), meta = (BlueprintSpawnableComponent))
class UPawnInitializationComponent : public UNelaricPawnComponent
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
	GAMEPLAYRUNTIME_API bool TryInitializePawn();

	/** @brief Invalidates context and restarts local initialization.
	 * @details Call on the game thread after controller, player state, input,
	 * or other required context changes. Revokes pawn Ready and invalidates
	 * all registered participants on this pawn, including pending work.
	 * Calls during initialization are applied after the current callback.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Pawn|Initialization")
	GAMEPLAYRUNTIME_API void InvalidatePawnContext();

	/** @brief Subscribes to Ready and immediately calls if already Ready.
	 * @details Call on the game thread. The pawn owns the subscription until
	 * explicitly removed or this component is destroyed. Callbacks run on
	 * the game thread; bind a UObject method to avoid dangling listeners.
	 * @param Callback Listener receiving this initialization component.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Pawn|Initialization")
	GAMEPLAYRUNTIME_API void RegisterAndCallPawnInitialized(FPawnInitializationCallback Callback);

	/** @brief Subscribes to future pawn readiness revocations.
	 * @details Call on the game thread. Register before starting gameplay;
	 * use RegisterAndCallPawnInitialized to handle late Ready subscription.
	 * @param Callback Game-thread listener stopped by removal or destruction.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Pawn|Initialization")
	GAMEPLAYRUNTIME_API void RegisterPawnInitializationRevoked(FPawnInitializationCallback Callback);

	/** @brief Removes a listener from both readiness notifications.
	 * @details Call on the game thread before abandoning a subscription.
	 * @param Callback Previously registered UObject listener.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Pawn|Initialization")
	GAMEPLAYRUNTIME_API void UnregisterPawnInitializationCallback(FPawnInitializationCallback Callback);

	/** @brief Reports whether this pawn is currently Ready.
	 *
	 * @details Call on the game thread. Returns false before BeginPlay and
	 * after EndPlay. This state is not replicated.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Pawn|Initialization")
	GAMEPLAYRUNTIME_API bool IsPawnInitialized() const;

	/** @brief Checks whether the pawn's required context is available.
	 *
	 * @details Called on the game thread by TryInitializePawn after BeginPlay.
	 * The default accepts any valid owning pawn. Override in C++ or Blueprint
	 * to require more context. Keep this query free of side effects. Use
	 * InvalidatePawnContext when bindings or required objects change.
	 * @return Whether initialization may complete now.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Nelaric|Pawn|Initialization")
	GAMEPLAYRUNTIME_API bool CanInitializePawn() const;

	/** @brief Replaces the local component configuration as one new round.
	 * @details Call on the game thread. During play, this revokes current Ready,
	 * destroys managed instances, validates the new asset, and tries readiness.
	 * A null asset creates no components and uses only CanInitializePawn.
	 * @param NewConfig Asset to use for the next local initialization round.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Pawn|Initialization")
	GAMEPLAYRUNTIME_API void SetInitializationConfig(UPawnInitializationConfig* NewConfig);

	/** @brief Broadcasts on each transition into Ready on this machine.
	 *
	 * @details Bind before BeginPlay to observe immediate initialization.
	 * Late listeners can query IsPawnInitialized(). Runs on the game thread.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Nelaric|Pawn|Initialization")
	FPawnInitialized OnPawnInitialized;

	/** @brief Broadcasts when current pawn Ready is revoked.
	 * @details Required participant invalidation, configuration replacement,
	 * and EndPlay revoke Ready on the game thread.
	 */
	UPROPERTY(BlueprintAssignable, Category = "Nelaric|Pawn|Initialization")
	FPawnInitializationRevoked OnPawnInitializationRevoked;

	/** @brief Asset that controls local component creation and readiness.
	 * @details Set on a Pawn or Character derived Blueprint. IDs, dependencies,
	 * creation sides, and required flags come only from this asset.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nelaric|Pawn|Initialization")
	TObjectPtr<UPawnInitializationConfig> InitializationConfig;

public:
	GAMEPLAYRUNTIME_API UPawnInitializationComponent(const FObjectInitializer& ObjectInitializer);
	bool IsConfiguredInstance(FName ComponentId, const UActorComponent* Component) const;
	bool HasConfiguredId(FName ComponentId) const;
	GAMEPLAYRUNTIME_API virtual void BeginPlay() override;
	GAMEPLAYRUNTIME_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	GAMEPLAYRUNTIME_API virtual bool CanInitializePawn_Implementation() const;

private:
	friend class UInitStateWorldSubsystem;
	friend class ANelaricPawn;
	friend class ANelaricCharacter;
	void BeginPawnContextChange();
	void EndPawnContextChange();
	int32 ContextChangeDepth = 0;
	bool bNotifyingRevocation = false;
	bool TryInitializationPass();
	bool bRefreshPending = false;
	bool bContextResetRequested = false;
	bool bPawnInitialized = false;
	bool bInitializationInProgress = false;
	bool bInitializationAllowed = false;
	bool bConfigValid = true;
	UPROPERTY(Transient)
	TObjectPtr<UPawnInitializationConfig> ActiveConfig;
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
