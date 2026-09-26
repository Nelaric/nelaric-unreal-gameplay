// Copyright (c) 2026 Nelaric

/** @file NelaricPawn.h
 * Declares the project's general-purpose pawn base.
 */

#pragma once

#include "GameFramework/Pawn.h"

#include "NelaricPawn.generated.h"

/** @brief Base pawn for project-specific controllable actors.
 *
 * @details Derive in C++ or Blueprint and compose gameplay with pawn
 * components. Retains APawn defaults, including its movement behavior.
 * The world manages actor lifetime. Access on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricPawn : public APawn
{
	GENERATED_BODY()

public:
	/** @brief Constructs the pawn with the engine's default behavior.
	 *
	 * @details Called by Unreal on the game thread during actor creation.
	 * @param ObjectInitializer Initializer supporting derived subobjects.
	 */
	NELARICFOUNDATION_API ANelaricPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
