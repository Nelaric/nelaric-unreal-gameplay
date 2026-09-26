// Copyright (c) 2026 Nelaric

/** @file NelaricCharacter.h
 * Declares the project's character base with native character movement.
 */

#pragma once

#include "GameFramework/Character.h"

#include "NelaricCharacter.generated.h"

/** @brief Base character for project-specific walking characters.
 *
 * @details Derive in C++ or Blueprint and compose gameplay with pawn
 * components. Retains ACharacter's capsule, mesh, movement component,
 * and engine defaults. The world manages actor lifetime.
 * Access on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/** @brief Constructs the character with its native default subobjects.
	 *
	 * @details Called by Unreal on the game thread during actor creation.
	 * @param ObjectInitializer Initializer supporting derived subobjects.
	 */
	NELARICFOUNDATION_API ANelaricCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
