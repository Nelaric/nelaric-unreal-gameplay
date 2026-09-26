// Copyright (c) 2026 Nelaric

/** @file NelaricSandboxTemplatePlayerController.h
 * Declares the building sandbox player controller template.
 */

#pragma once

#include "Player/NelaricPlayerController.h"

#include "NelaricSandboxTemplatePlayerController.generated.h"

/** @brief Player-owned controller for the building sandbox template.
 *
 * @details The owning client and server each have an instance while
 * connected. Derive it to add input and player requests on the game thread.
 * Inherited session transition behavior remains available.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricSandboxPlayerController : public ANelaricPlayerController
{
	GENERATED_BODY()
};
