// Copyright (c) 2026 Nelaric Contributors

/** @file PlayerInputComponent.h
 * Declares an initialization participant for local pawn input.
 */

#pragma once

#include "Delegates/Delegate.h"
#include "Input/NelaricInputComponent.h"
#include "Pawn/PawnInitStateComponent.h"

#include "PlayerInputComponent.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UNelaricInputConfig;
class UPlayerInputComponent;

namespace Nelaric
{
/// Announces that a local player's input can be bound on the game thread.
DECLARE_MULTICAST_DELEGATE_OneParam(FPlayerInputReady, UPlayerInputComponent*);

/// Announces that the previous local input bindings have been revoked.
DECLARE_MULTICAST_DELEGATE_OneParam(FPlayerInputRevoked, UPlayerInputComponent*);
} // namespace Nelaric

/** @brief Coordinates a pawn's local input through the init-state lifecycle.
 *
 * @details Add this component to a pawn's initialization configuration.
 * The pawn owns it; UPawnInitStateComponent drives preparation, Ready, and
 * invalidation. On Ready it installs configured mapping contexts and calls
 * BindInputActions for game-defined callbacks. Invalidation removes tracked
 * callbacks and mappings. Remote pawns and AI reach Ready without local
 * input work. All operations and notifications use the game thread.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Nelaric), meta = (BlueprintSpawnableComponent))
class UPlayerInputComponent : public UPawnInitStateComponent
{
	GENERATED_BODY()

public:
	/** @brief Replaces the input asset and starts a new init generation.
	 *
	 * @details Call on the game thread. Null disables local bindings while
	 * allowing this participant to enter Ready. The pawn owns this component;
	 * it retains the new asset until replaced or destroyed.
	 * @param NewConfig Asset used by the next local input generation.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Input")
	GAMEPLAYRUNTIME_API void SetInputConfig(UNelaricInputConfig* NewConfig);

	/** @brief Returns the currently bound Enhanced Input component.
	 *
	 * @details Call on the game thread. The result is non-owning and becomes
	 * null after input revocation, controller change, or component removal.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Input")
	UNelaricInputComponent* GetBoundInputComponent() const
	{
		return BoundInputComponent.Get();
	}

	/** @brief Observes local input readiness after game-defined bindings.
	 * @details Bind on the game thread. The
	 * component owns this native
	 * delegate; remove listeners before they outlive the component.
	 * @return
	 * Delegate announcing each local Ready entry.
	 */
	Nelaric::FPlayerInputReady& OnPlayerInputReady()
	{
		return PlayerInputReady;
	}

	/** @brief Observes local input revocation before bindings are removed.
	 * @details Bind on the game thread. The
	 * component owns this native
	 * delegate; remove listeners before they outlive the component.
	 * @return
	 * Delegate announcing each active generation's cleanup.
	 */
	Nelaric::FPlayerInputRevoked& OnPlayerInputRevoked()
	{
		return PlayerInputRevoked;
	}

	/// Asset assigned before play or replaced through SetInputConfig.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Nelaric|Input")
	TObjectPtr<UNelaricInputConfig> InputConfig;

public:
	GAMEPLAYRUNTIME_API UPlayerInputComponent(const FObjectInitializer& ObjectInitializer);

protected:
	/** @brief Waits for local input objects when this pawn has an input asset.
	 * @details Remote and unconfigured pawns prepare without local bindings.
	 */
	GAMEPLAYRUNTIME_API virtual bool CanEntryDataAvailable() override;

	/** @brief Waits for the owning local player's Enhanced Input subsystem.
	 * @details A later pawn context invalidation retries missing input.
	 */
	GAMEPLAYRUNTIME_API virtual bool CanEntryDataInitialized() override;

	/** @brief Installs mappings and invites the game to bind native actions.
	 * @details Called after the initialization group commits Ready.
	 */
	GAMEPLAYRUNTIME_API virtual void OnInitReady() override;

	/** @brief Removes callbacks and contexts from the previous generation.
	 * @param Previous Snapshot before the init-state invalidation.
	 */
	GAMEPLAYRUNTIME_API virtual void OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot& Previous) override;

	/** @brief Binds game-defined actions after mappings become active.
	 *
	 * @details Override in a game subclass on the game thread. Use the
	 * protected BindNativeAction helper so generation invalidation removes
	 * every binding. The default implementation binds no actions.
	 */
	virtual void BindInputActions()
	{
	}

	/** @brief Releases game-specific state before native bindings are removed.
	 * @details Override in a game subclass. Called once for each active local
	 * generation, on the game thread, including component teardown.
	 */
	virtual void UnbindInputActions()
	{
	}

	/** @brief Binds one game action and tracks its removal handle.
	 * @details Call from BindInputActions on the game thread. Returns false
	 * for an absent tag, action, config, or callback target.
	 * @tparam UserClass UObject callback owner type.
	 * @tparam FuncType Callback type accepted by Enhanced Input.
	 * @param InputTag Exact tag supplied by the game.
	 * @param TriggerEvent Event that invokes the callback.
	 * @param Object Non-owning callback target.
	 * @param Func Member callback to bind.
	 * @return Whether a binding was created in the current generation.
	 */
	template <class UserClass, typename FuncType>
	    requires Nelaric::Input::CNativeActionCallback<UserClass, FuncType>
	FORCEINLINE bool BindNativeAction(const FGameplayTag& InputTag, ETriggerEvent TriggerEvent, UserClass* Object,
	                                  FuncType Func)
	{
		if (UNelaricInputComponent* Component = BoundInputComponent.Get())
		{
			return Component->BindNativeAction(InputConfig, InputTag, TriggerEvent, Object, Func, BindHandles);
		}
		ReportMissingInputComponent(InputTag);
		return false;
	}

private:
	GAMEPLAYRUNTIME_API void ReportMissingInputComponent(const FGameplayTag& InputTag) const;
	void ReleaseLocalInput();
	TWeakObjectPtr<UNelaricInputComponent> BoundInputComponent;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BoundSubsystem;
	TArray<uint32> BindHandles;
	Nelaric::FPlayerInputReady PlayerInputReady;
	Nelaric::FPlayerInputRevoked PlayerInputRevoked;
	bool bLocalInputActive = false;
};
