// Copyright (c) 2026 Nelaric

/** @file NelaricOpenWorldTemplatePlayerController.h
 * Declares the open-world exploration player controller template.
 */

#pragma once

#include "Player/NelaricPlayerController.h"

#include "NelaricOpenWorldTemplatePlayerController.generated.h"

/** @brief Player-owned controller for the open-world exploration template.
 *
 * @details The owning client and server each have an instance while
 * connected. Derive it to add input and player requests on the game thread.
 * Inherited session transition behavior remains available.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricOpenWorldPlayerController : public ANelaricPlayerController
{
	GENERATED_BODY()
};
