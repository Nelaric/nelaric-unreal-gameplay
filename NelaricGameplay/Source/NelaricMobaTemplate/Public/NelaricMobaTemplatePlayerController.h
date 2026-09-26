// Copyright (c) 2026 Nelaric

/** @file NelaricMobaTemplatePlayerController.h
 * Declares the MOBA player controller template.
 */

#pragma once

#include "Player/NelaricPlayerController.h"

#include "NelaricMobaTemplatePlayerController.generated.h"

/** @brief Player-owned controller for the MOBA template.
 *
 * @details The owning client and server each have an instance while
 * connected. Derive it to add input and player requests on the game thread.
 * Inherited session transition behavior remains available.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricMobaPlayerController : public ANelaricPlayerController
{
	GENERATED_BODY()
};
