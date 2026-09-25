// Copyright (c) 2026 Nelaric

/** @file NelaricWorldSettings.h
 * Declares the project World Settings base class.
 */

#pragma once

#include "GameFramework/WorldSettings.h"

#include "NelaricWorldSettings.generated.h"

/** @brief Base World Settings class for NelaricGameplay maps.
 *
 * @details Gameplay-specific World Settings classes must derive from
 * this class. The owning level retains its instance while loaded.
 * The base class deliberately carries no startup configuration; world
 * startup configuration is selected before a map is loaded. Access
 * World Settings on the game thread like other world actors.
 */
UCLASS(Blueprintable)
class NELARICGAMEPLAYCORE_API ANelaricWorldSettings : public AWorldSettings
{
	GENERATED_BODY()
};
