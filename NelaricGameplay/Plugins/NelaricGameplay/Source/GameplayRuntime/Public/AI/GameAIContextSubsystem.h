// Copyright (c) 2026 Nelaric Contributors

/** @file GameAIContextSubsystem.h */

#pragma once

#include "Subsystems/WorldSubsystem.h"

#include "GameAIContextSubsystem.generated.h"

/** @brief World-owned extension point for shared AI services.
 *
 * @details Unreal creates and destroys this service with its world.
 * Native subclasses may add game services and be selected on the Schema.
 * The base instance exposes no game rules. Access only on the game thread.
 */
UCLASS(MinimalAPI, BlueprintType)
class UGameAIContextSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
};
