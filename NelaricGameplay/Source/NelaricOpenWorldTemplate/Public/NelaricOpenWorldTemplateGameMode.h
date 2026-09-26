// Copyright (c) 2026 Nelaric

/** @file NelaricOpenWorldTemplateGameMode.h
 * Declares the open-world exploration game mode template.
 */

#pragma once

#include "NelaricGameModeBase.h"

#include "NelaricOpenWorldTemplateGameMode.generated.h"

/** @brief World entry point for the open-world exploration template.
 *
 * @details Derive this game mode to keep world progress
 * independent of local activities and
 * persistence adapters. This base adds no gameplay rules. The server
 * owns this actor for the world lifetime; gameplay runs on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricOpenWorldGameMode : public ANelaricGameModeBase
{
	GENERATED_BODY()

public:
	/// Selects this template's controller and player state on the server.
	ANelaricOpenWorldGameMode();
};
