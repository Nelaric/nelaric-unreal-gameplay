// Copyright (c) 2026 Nelaric
/** @file NelaricInputComponent.h
 * Provides tracked native action bindings.
 */
#pragma once
#include "EnhancedInputComponent.h"
#include "Input/NelaricInputConfig.h"
#include "NelaricInputComponent.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;

/** @brief Enhanced Input component with native bindings addressed by tags.
 * @details Unreal creates this component for the pawn input stack.
 * Use on the game thread. Bindings retain weak UObject targets.
 */
UCLASS(MinimalAPI, Config = Input)
class UNelaricInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
public:
	/** @brief Replaces this component's active mapping configuration.
	 * @details Call on the game thread. Releases previously owned contexts.
	 * Contexts already active are borrowed with their current priority;
	 * only newly activated contexts are removed by RemoveInputMappings.
	 * Registered remapping rows persist for the local player.
	 * @param InputConfig Asset describing mappings; null leaves no mappings.
	 * @param InputSubsystem Local player's subsystem; null leaves no mappings.
	 * @return Whether a non-null config and subsystem were accepted.
	 */
	GAMEPLAYRUNTIME_API bool AddInputMappings(const UNelaricInputConfig* InputConfig,
	                                          UEnhancedInputLocalPlayerSubsystem* InputSubsystem);
	/** @brief Releases only mapping contexts activated by this component.
	 * @details Call on the game thread. Safe to repeat. Does not alter action
	 * bindings or unregister persistent player remapping rows.
	 */
	GAMEPLAYRUNTIME_API void RemoveInputMappings();

	/** @brief Binds a native action and appends its removal handle.
	 * @details Call on the game thread. Missing config, action, or target
	 * returns false without changing handles. The caller owns the handles.
	 * @tparam UserClass UObject callback owner type.
	 * @tparam FuncType Callback type accepted by Enhanced Input.
	 * @param InputConfig Configuration retaining the action.
	 * @param InputTag Exact tag to resolve.
	 * @param TriggerEvent Event that invokes the callback on the game thread.
	 * @param Object Non-owning callback target.
	 * @param Func Member callback to bind.
	 * @param BindHandles Output collection of created binding handles.
	 * @return Whether a binding was created.
	 */
	template <class UserClass, typename FuncType>
	FORCEINLINE bool BindNativeAction(const UNelaricInputConfig* InputConfig, const FGameplayTag& InputTag,
	                                  ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func,
	                                  TArray<uint32>& BindHandles)
	{
		if (InputConfig && Object && Func)
		{
			if (const UInputAction* Action = InputConfig->FindNativeInputActionForTag(InputTag))
			{
				BindHandles.Add(BindAction(Action, TriggerEvent, Object, Func).GetHandle());
				return true;
			}
		}
		return false;
	}
	/** @brief Removes tracked bindings and empties their handles.
	 * @details Call on the game thread on the component that created them.
	 * Unknown handles are ignored. Other bindings remain intact.
	 * @param BindHandles Handles consumed by this call.
	 */
	GAMEPLAYRUNTIME_API void RemoveBinds(TArray<uint32>& BindHandles);

public:
	virtual void OnUnregister() override;

private:
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputMappingContext>> OwnedMappingContexts;
};
