// Copyright (c) 2026 Nelaric Contributors
/** @file NelaricInputComponent.h
 * Provides tracked native action bindings.
 */
#pragma once
#include "EnhancedInputComponent.h"
#include "Input/NelaricInputConfig.h"

#include <concepts>

#include "NelaricInputComponent.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
class UInputMappingContext;

namespace Nelaric::Input
{
/** @brief Accepts native action callbacks supported by Enhanced Input.
 * @tparam UserClass UObject target retained weakly by the input binding.
 * @tparam FuncType Nullable member callback matching a handler signature.
 */
template <class UserClass, class FuncType>
concept CNativeActionCallback =
    std::derived_from<UserClass, UObject> && requires(UEnhancedInputComponent& Component, const UInputAction* Action,
                                                      ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func) {
	    { !Func } -> std::convertible_to<bool>;
	    { Component.BindAction(Action, TriggerEvent, Object, Func) } -> std::same_as<FEnhancedInputActionEventBinding&>;
    };
} // namespace Nelaric::Input

/** @brief Enhanced Input component with native bindings addressed by tags.
 * @details Unreal creates this component for the pawn input stack.
 * Use on the game thread. Bindings retain weak UObject targets.
 */
UCLASS(MinimalAPI, Config = Input)
class UNelaricInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()
public:
	/** @brief Installs the input mapping config.
	 * @details On the game thread, replace owned contexts; activate
	 * entries
	 * marked bActivateOnStart; borrow already active contexts
	 * @param InputConfig Asset or null to
	 * clear
	 * @param InputSubsystem Local subsystem or null to clear
	 * @return True when both inputs are
	 * present
	 */
	GAMEPLAYRUNTIME_API bool AddInputMappings(const UNelaricInputConfig* InputConfig,
	                                          UEnhancedInputLocalPlayerSubsystem* InputSubsystem);
	/** @brief Activates a mapping by exact tag.
	 * @details Call on the game thread after setup.
	 * An active
	 * context keeps its priority.
	 * A new context is owned until removal or reset.
	 * @param MappingTag Exact tag
	 * to resolve.
	 * @return True when the context is active.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Input")
	GAMEPLAYRUNTIME_API bool AddInputMappingByTag(const FGameplayTag& MappingTag);
	/** @brief Removes an owned context by exact tag.
	 * @details Call on the game thread.
	 * Borrowed contexts and
	 * remapping rows remain.
	 * @param MappingTag Exact tag to resolve.
	 * @return True when an active owned context
	 * was removed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Nelaric|Input")
	GAMEPLAYRUNTIME_API bool RemoveInputMappingByTag(const FGameplayTag& MappingTag);
	/** @brief Releases mapping contexts owned by this component.
	 * @details Call on the game thread. Safe to
	 * repeat.
	 * Action bindings and player remapping rows remain.
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
	    requires Nelaric::Input::CNativeActionCallback<UserClass, FuncType>
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
		ReportNativeBindingFailure(InputConfig, InputTag, TriggerEvent);
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
	GAMEPLAYRUNTIME_API void ReportNativeBindingFailure(const UNelaricInputConfig* InputConfig,
	                                                    const FGameplayTag& InputTag, ETriggerEvent TriggerEvent) const;
	bool ActivateMapping(const FNelaricInputMapping& Mapping, UEnhancedInputLocalPlayerSubsystem* InputSubsystem);
	UPROPERTY(Transient)
	TObjectPtr<const UNelaricInputConfig> ActiveInputConfig;
	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> MappingSubsystem;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInputMappingContext>> OwnedMappingContexts;
};
