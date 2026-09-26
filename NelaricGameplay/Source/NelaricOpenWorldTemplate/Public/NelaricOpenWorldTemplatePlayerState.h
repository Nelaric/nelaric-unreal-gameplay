// Copyright (c) 2026 Nelaric

/** @file NelaricOpenWorldTemplatePlayerState.h
 * Declares the open-world exploration player state template.
 */

#pragma once

#include "GameFramework/PlayerState.h"

#include "NelaricOpenWorldTemplatePlayerState.generated.h"

/** @brief Replicated player state for the open-world exploration template.
 *
 * @details The server creates this actor for a participating player.
 * Derive it to add client-visible player state with Unreal replication.
 * Read and write gameplay state on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricOpenWorldPlayerState : public APlayerState
{
	GENERATED_BODY()
};
