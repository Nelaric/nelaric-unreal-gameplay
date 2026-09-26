// Copyright (c) 2026 Nelaric

/** @file NelaricPlayerController.h
 * Declares the project's player-owned network endpoint.
 */

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "GameFramework/PlayerController.h"

#include "NelaricPlayerController.generated.h"

/// Reports the current server's departure decision on the game thread.
DECLARE_MULTICAST_DELEGATE_ThreeParams(FNelaricDepartureDecision, uint64, const FString&, bool);

/** @brief Base player controller for NelaricGameplay worlds.
 *
 * @details The owning client and current server each have an instance
 * while connected. Game-specific controllers may derive from this class.
 * The current server approves a valid departure request immediately.
 * Destination admission uses a separate pre-travel connection.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** @brief Asks the current server to approve leaving its session.
	 *
	 * @details Call on the game thread from the owning local controller.
	 * The server directly approves a valid request and returns its ID and
	 * destination through OnDepartureDecision on the game thread. Sending
	 * a request does not authorize travel or destination admission.
	 *
	 * @param RequestId Identity of the pending transition; must be nonzero.
	 * @param TargetAddress Destination server URL or host address.
	 * @return True if the request was sent, false for invalid local input.
	 */
	NELARICFOUNDATION_API bool RequestDepartureApproval(uint64 RequestId, const FString& TargetAddress);

	/** @brief Observes decisions returned by the current server.
	 *
	 * @details Bind on the game thread. The controller owns the delegate;
	 * bindings cease to be useful when its connection or world is torn down.
	 *
	 * @return Delegate reporting identity, destination, and approval.
	 */
	NELARICFOUNDATION_API FNelaricDepartureDecision& OnDepartureDecision();

public:
	// Native subclasses in other modules need these virtual definitions.
	NELARICFOUNDATION_API virtual void ServerRequestDepartureApproval_Implementation(uint64 RequestId,
	                                                                                 const FString& TargetAddress);
	NELARICFOUNDATION_API virtual void
	ClientReceiveDepartureDecision_Implementation(uint64 RequestId, const FString& TargetAddress, bool bApproved);

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestDepartureApproval(uint64 RequestId, const FString& TargetAddress);

	UFUNCTION(Client, Reliable)
	void ClientReceiveDepartureDecision(uint64 RequestId, const FString& TargetAddress, bool bApproved);

	FNelaricDepartureDecision DepartureDecision;
};
