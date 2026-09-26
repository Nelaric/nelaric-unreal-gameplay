// Copyright (c) 2026 Nelaric

/** @file NelaricTransitionBeaconClient.h
 * Declares the destination server's pre-travel approval channel.
 */

#pragma once

#include "CoreTypes.h"
#include "Core/NetWork/NelaricNetworkEndpoint.h"
#include "Delegates/Delegate.h"
#include "OnlineBeaconClient.h"

#include "NelaricTransitionBeaconClient.generated.h"

/** @brief Reports a target decision or beacon failure on the game thread.
 *
 * @details Parameters are request ID, approval, and whether the target
 * authority replied. A connection failure has false for both bools.
 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FNelaricTargetDecision, uint64, bool, bool);

/** @brief Contacts a destination server before game travel.
 *
 * @details The requesting client owns this beacon until it receives a
 * decision or failure. The destination server decides whether to admit
 * the request; a valid response is bound to the supplied request ID.
 */
UCLASS(Transient, NotPlaceable)
class NELARICFOUNDATION_API ANelaricTransitionBeaconClient : public AOnlineBeaconClient
{
	GENERATED_BODY()

public:
	/** @brief Connects to a target's beacon and requests admission.
	 *
	 * @details Call on the game thread with the target beacon endpoint.
	 *
	 * @param Endpoint Destination approval beacon address and port.
	 * @param RequestId Nonzero identity of the pending transition.
	 * @return True if a beacon connection attempt was started.
	 */
	bool BeginTargetApproval(const UE::Nelaric::FNetworkEndpoint& Endpoint, uint64 RequestId);

	/** @brief Observes the destination server's decision or failure.
	 *
	 * @details Bind before BeginTargetApproval. The final bool is false
	 * when the beacon failed before the target authority replied. The
	 * owner must also enforce a timeout.
	 *
	 * @return Delegate carrying identity, approval, and reply presence.
	 */
	FNelaricTargetDecision& OnTargetDecision();

	/** @brief Sends the approval request after beacon connection.
	 *
	 * @details Unreal calls this on the game thread.
	 */
	virtual void OnConnected() override;

	/** @brief Reports a failed beacon connection.
	 *
	 * @details Unreal calls this on the game thread.
	 */
	virtual void OnFailure() override;

private:
	UFUNCTION(Server, Reliable)
	void ServerRequestTargetApproval(uint64 RequestId);

	UFUNCTION(Client, Reliable)
	void ClientReceiveTargetDecision(uint64 RequestId, bool bApproved);

	uint64 PendingRequestId = 0;
	bool bDecisionDelivered = false;
	FNelaricTargetDecision TargetDecision;
};
