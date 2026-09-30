// Copyright (c) 2026 Nelaric Contributors

/** @file TransitionBeaconHost.h
 * Declares the destination approval beacon registration.
 */

#pragma once

#include "OnlineBeaconHostObject.h"

#include "TransitionBeaconHost.generated.h"

/** @brief Registers the target approval beacon client type.
 *
 * @details The authoritative world owns this actor while accepting
 * pre-travel transition requests. The game mode makes each decision.
 */
UCLASS(MinimalAPI, Transient, NotPlaceable)
class ATransitionBeaconHost : public AOnlineBeaconHostObject
{
	GENERATED_BODY()

public:
	/// Selects the matching beacon client type and network identifier.
	GAMEPLAYRUNTIME_API ATransitionBeaconHost(const FObjectInitializer& ObjectInitializer);
};
