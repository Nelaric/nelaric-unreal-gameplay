// Copyright (c) 2026 Nelaric

/** @file NelaricBattleRoyaleTemplateGameMode.h
 * Declares the battle royale game mode template.
 */

#pragma once

#include "NelaricGameModeBase.h"

#include "NelaricBattleRoyaleTemplateGameMode.generated.h"

/** @brief World entry point for the battle royale template.
 *
 * @details Derive this game mode to own bounded rounds,
 * participants, eliminations, phases,
 * and results in an activity. This base adds no gameplay rules. The server
 * owns this actor for the world lifetime; gameplay runs on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricBattleRoyaleGameMode : public ANelaricGameModeBase
{
	GENERATED_BODY()

public:
	/// Selects this template's controller and player state on the server.
	ANelaricBattleRoyaleGameMode();
};
