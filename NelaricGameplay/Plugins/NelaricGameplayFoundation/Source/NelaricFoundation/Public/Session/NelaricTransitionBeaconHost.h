// Copyright (c) 2026 Nelaric

/** @file NelaricTransitionBeaconHost.h
 * Declares the destination approval beacon registration.
 */

#pragma once

#include "OnlineBeaconHostObject.h"

#include "NelaricTransitionBeaconHost.generated.h"

/** @brief Registers the target approval beacon client type.
 *
 * @details The authoritative world owns this actor while accepting
 * pre-travel transition requests. The game mode makes each decision.
 */
UCLASS(Transient, NotPlaceable)
class NELARICFOUNDATION_API ANelaricTransitionBeaconHost : public AOnlineBeaconHostObject
{
	GENERATED_BODY()

public:
	/// Selects the matching beacon client type and network identifier.
	ANelaricTransitionBeaconHost(const FObjectInitializer& ObjectInitializer);
};
