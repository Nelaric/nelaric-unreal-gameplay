// Copyright (c) 2026 Nelaric Contributors

/** @file DemoPlayerInputComponent.h
 * Declares the player input extension point for DemoGame.
 */

#pragma once

#include "Input/PlayerInputComponent.h"

#include "DemoPlayerInputComponent.generated.h"

/** @brief Provides a pawn-owned player input extension point for DemoGame.
 *
 * @details Add this class through pawn initialization. The inherited
 * lifecycle manages configured mapping contexts on the game thread.
 * This class supplies no concrete input callbacks or action bindings.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Demo), meta = (BlueprintSpawnableComponent))
class UDemoPlayerInputComponent : public UPlayerInputComponent
{
	GENERATED_BODY()

public:
	/** @brief Constructs the demo input participant.
	 *
	 * @details Called by Unreal on the game thread during component creation.
	 *
	 * @param ObjectInitializer Initializer for inherited component state.
	 */
	DEMOGAME_API UDemoPlayerInputComponent(const FObjectInitializer& ObjectInitializer);
};
