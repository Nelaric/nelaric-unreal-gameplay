// Copyright (c) 2026 Nelaric

/** @file NelaricMobaTemplatePlayerState.h
 * Declares the MOBA player state template.
 */

#pragma once

#include "GameFramework/PlayerState.h"

#include "NelaricMobaTemplatePlayerState.generated.h"

/** @brief Replicated player state for the MOBA template.
 *
 * @details The server creates this actor for a participating player.
 * Derive it to add client-visible player state with Unreal replication.
 * Read and write gameplay state on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricMobaPlayerState : public APlayerState
{
	GENERATED_BODY()
};
