// Copyright (c) 2026 Nelaric

/** @file NelaricSandboxTemplateGameMode.h
 * Declares the building sandbox game mode template.
 */

#pragma once

#include "NelaricGameModeBase.h"

#include "NelaricSandboxTemplateGameMode.generated.h"

/** @brief World entry point for the building sandbox template.
 *
 * @details Derive this game mode to keep world events
 * and player state independent of a global
 * goal or end state. This base adds no gameplay rules. The server
 * owns this actor for the world lifetime; gameplay runs on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricSandboxGameMode : public ANelaricGameModeBase
{
	GENERATED_BODY()

public:
	/// Selects this template's controller and player state on the server.
	ANelaricSandboxGameMode();
};
