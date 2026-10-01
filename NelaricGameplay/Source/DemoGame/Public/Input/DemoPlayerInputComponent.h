// Copyright (c) 2026 Nelaric Contributors

/** @file DemoPlayerInputComponent.h
 * Declares the player input participant for DemoGame.
 */

#pragma once

#include "Input/PlayerInputComponent.h"

#include "DemoPlayerInputComponent.generated.h"

struct FInputActionValue;

/** @brief Binds native input for DemoGame players.
 * @details Add this class through pawn initialization. It binds
 * Move,
 * mouse and stick Look, and Jump using the assigned input config.
 * The pawn owns it. Input runs on the game
 * thread and follows the
 * init-state lifecycle.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Demo), meta = (BlueprintSpawnableComponent))
class UDemoPlayerInputComponent : public UPlayerInputComponent
{
	GENERATED_BODY()

public:
	/** @brief Constructs the demo input participant.
	 * @details Called by Unreal on the game thread during component
	 * creation.
	 * @param ObjectInitializer Initializer for inherited component state.
	 */
	DEMOGAME_API UDemoPlayerInputComponent(const FObjectInitializer& ObjectInitializer);

protected:
	/** @brief Binds demo movement, camera, and jump actions for local input.
	 * @details The base class tracks each
	 * binding for generation cleanup.
	 */
	virtual void BindInputActions() override;

	/// Stops held jump input when the input generation ends.
	virtual void UnbindInputActions() override;

private:
	void InputMove(const FInputActionValue& Value);
	void InputLookMouse(const FInputActionValue& Value);
	void InputLookStick(const FInputActionValue& Value);
	void InputJumpStarted();
	void InputJumpStopped();
};
