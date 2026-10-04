// Copyright (c) 2026 Nelaric Contributors

/** @file DemoPlayerInputComponent.h
 * Declares the player input participant for DemoGame.
 */

#pragma once

#include "Input/PlayerInputComponent.h"
#include "Engine/EngineTypes.h"

#include "DemoPlayerInputComponent.generated.h"

struct FInputActionValue;
class ADemoCharacter;
class ADemoOverviewPawn;

/** @brief Binds independent character and overview input.
 * @details The pawn owns this component. One shared config
 * selects separate
 * mapping callbacks on the game thread. Revocation stops held actions and
 * removes the previous
 * input generation.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Demo), meta = (BlueprintSpawnableComponent))
class UDemoPlayerInputComponent : public UPlayerInputComponent
{
	GENERATED_BODY()

public:
	/// Returns the active mapping tag, or an invalid tag without local input.
	UFUNCTION(BlueprintPure, Category = "Demo|Input")
	FGameplayTag GetActiveInputMappingTag() const
	{
		return ActiveMappingTag;
	}

	/// Overview keyboard speed in centimeters per second; zero disables motion.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Input", meta = (ClampMin = "0.0"))
	float OverviewMoveSpeed = 1200.0f;

	/// Horizontal centimeters per mouse delta unit while dragging the overview.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Input", meta = (ClampMin = "0.0"))
	float OverviewDragSensitivity = 10.0f;

	/// Collision object type used for the overview's cursor selection query.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Input")
	TEnumAsByte<EObjectTypeQuery> SelectionObjectType;

public:
	/** @brief Constructs the demo input participant.
	 * @details Called by Unreal on the game thread during component
	 * creation.
	 * @param ObjectInitializer Initializer for inherited component state.
	 */
	DEMOGAME_API UDemoPlayerInputComponent(const FObjectInitializer& ObjectInitializer);

protected:
	/** @brief Activates one mapping profile and binds its own callbacks.
	 * @details Runs on the game thread after
	 * the base installs the shared
	 * configuration. The base tracks every binding for generation cleanup.
	 */
	virtual void BindInputActions() override;

	/// Stops held jump and drag input when the input generation ends.
	virtual void UnbindInputActions() override;

private:
	UPROPERTY(Transient)
	FGameplayTag ActiveMappingTag;
	bool bOverviewDragging = false;

	bool HasActiveMapping(const FGameplayTag& MappingTag) const;
	ADemoCharacter* GetInputCharacter() const;
	ADemoOverviewPawn* GetInputOverview() const;
	void InputMove(const FInputActionValue& Value);
	void InputLookMouse(const FInputActionValue& Value);
	void InputLookStick(const FInputActionValue& Value);
	void InputJumpStarted();
	void InputJumpStopped();
	void InputOverviewMove(const FInputActionValue& Value);
	void InputOverviewDrag(const FInputActionValue& Value);
	void InputOverviewDragStopped();
	void InputOverviewClick(const FInputActionValue& Value);
	void InputReturnOverview();
	void InputUnarmed();
	void InputPrimaryWeapon();
};
