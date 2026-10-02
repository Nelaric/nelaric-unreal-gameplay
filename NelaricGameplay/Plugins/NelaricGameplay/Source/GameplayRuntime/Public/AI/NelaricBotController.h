// Copyright (c) 2026 Nelaric Contributors

/** @file NelaricBotController.h
 * Declares a bot controller with its own participant player state.
 */

#pragma once

#include "AIController.h"

#include "NelaricBotController.generated.h"

/** @brief Supplies a player-state-bearing controller for pawn handbacks.
 *
 * @details The authority world owns this actor. Game-specific bot classes
 * may derive from it and add behavior. Possession operations use the game
 * thread. Brain logic stops before the pawn context is removed.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricBotController : public AAIController
{
	GENERATED_BODY()

public:
public:
	GAMEPLAYRUNTIME_API ANelaricBotController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	GAMEPLAYRUNTIME_API virtual void OnUnPossess() override;
};
