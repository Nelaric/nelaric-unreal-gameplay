// Copyright (c) 2026 Nelaric

/** @file FrameworkPerformanceSubsystem.h */

#pragma once

#include "Subsystems/GameInstanceSubsystem.h"

#include "FrameworkPerformanceSubsystem.generated.h"

/**
 * @brief Provides the game-instance scope for framework performance support.
 *
 * @details Unreal owns one instance per game instance. This initial type
 * defines no performance operations and does not own the editor HUD.
 */
UCLASS(MinimalAPI)
class UFrameworkPerformanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
};
