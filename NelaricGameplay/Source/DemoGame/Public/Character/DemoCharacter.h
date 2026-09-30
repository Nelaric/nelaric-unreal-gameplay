// Copyright (c) 2026 Nelaric Contributors

/** @file DemoCharacter.h
 * Declares the base character for DemoGame.
 */

#pragma once

#include "Pawn/NelaricCharacter.h"

#include "DemoCharacter.generated.h"

/** @brief Base character for game-specific demo characters.
 * @details Derive demo character variants in C++ or
 * Blueprint. The world
 * owns each instance, and character operations run on the game thread.
 * Pawn initialization
 * is inherited from ANelaricCharacter.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoCharacter : public ANelaricCharacter
{
	GENERATED_BODY()

public:
	/** @brief Constructs a demo character with inherited pawn initialization.
	 * @details Called by Unreal on the game
	 * thread when spawning the actor.
	 * @param ObjectInitializer Initializer for inherited default subobjects.
	 */
	DEMOGAME_API ADemoCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
