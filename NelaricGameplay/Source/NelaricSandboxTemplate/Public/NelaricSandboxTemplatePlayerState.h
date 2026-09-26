// Copyright (c) 2026 Nelaric

/** @file NelaricSandboxTemplatePlayerState.h
 * Declares the building sandbox player state template.
 */

#pragma once

#include "GameFramework/PlayerState.h"

#include "NelaricSandboxTemplatePlayerState.generated.h"

/** @brief Replicated player state for the building sandbox template.
 *
 * @details The server creates this actor for a participating player.
 * Derive it to add client-visible player state with Unreal replication.
 * Read and write gameplay state on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricSandboxPlayerState : public APlayerState
{
	GENERATED_BODY()
};
