// Copyright (c) 2026 Nelaric Contributors

/** @file DemoPlayerCharacter.h
 * Declares the player-character specialization for DemoGame.
 */

#pragma once

#include "Character/DemoCharacter.h"

#include "DemoPlayerCharacter.generated.h"

/** @brief Player-controlled character type for DemoGame.
 * @details Derive player-specific behavior in C++ or
 * Blueprint. The world
 * owns each instance, and character operations run on the game thread.
 * This class inherits
 * pawn initialization from ADemoCharacter.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoPlayerCharacter : public ADemoCharacter
{
	GENERATED_BODY()

public:
	/** @brief Constructs a player character with inherited pawn behavior.
	 * @details Called by Unreal on the game
	 * thread when spawning the actor.
	 * @param ObjectInitializer Initializer for inherited default subobjects.
	 */
	DEMOGAME_API ADemoPlayerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
