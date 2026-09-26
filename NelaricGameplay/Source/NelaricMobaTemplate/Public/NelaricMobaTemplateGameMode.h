// Copyright (c) 2026 Nelaric

/** @file NelaricMobaTemplateGameMode.h
 * Declares the MOBA game mode template.
 */

#pragma once

#include "NelaricGameModeBase.h"

#include "NelaricMobaTemplateGameMode.generated.h"

/** @brief World entry point for the MOBA template.
 *
 * @details Derive this game mode to own parallel objectives,
 * teams, optional scoring, and
 * victory in an activity. This base adds no gameplay rules. The server
 * owns this actor for the world lifetime; gameplay runs on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricMobaGameMode : public ANelaricGameModeBase
{
	GENERATED_BODY()

public:
	/// Selects this template's controller and player state on the server.
	ANelaricMobaGameMode();
};
