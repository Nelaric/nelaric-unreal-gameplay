// Copyright (c) 2026 Nelaric

/** @file NelaricBattleRoyaleTemplatePlayerController.h
 * Declares the battle royale player controller template.
 */

#pragma once

#include "Player/NelaricPlayerController.h"

#include "NelaricBattleRoyaleTemplatePlayerController.generated.h"

/** @brief Player-owned controller for the battle royale template.
 *
 * @details The owning client and server each have an instance while
 * connected. Derive it to add input and player requests on the game thread.
 * Inherited session transition behavior remains available.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricBattleRoyalePlayerController : public ANelaricPlayerController
{
	GENERATED_BODY()
};
