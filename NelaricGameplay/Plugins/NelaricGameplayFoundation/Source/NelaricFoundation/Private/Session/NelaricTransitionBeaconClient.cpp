// Copyright (c) 2026 Nelaric

#include "Session/NelaricTransitionBeaconClient.h"

#include "Engine/EngineBaseTypes.h"
#include "NelaricGameModeBase.h"

bool ANelaricTransitionBeaconClient::BeginTargetApproval(const UE::Nelaric::FNetworkEndpoint& Endpoint,
                                                         uint64 RequestId)
{
	if (RequestId == 0 || !Endpoint.IsValid() || PendingRequestId != 0)
	{
		return false;
	}

	const FString AddressWithPort = FString::Printf(TEXT("%s:%d"), *Endpoint.Address, Endpoint.Port);
	FURL BeaconURL(nullptr, *AddressWithPort, TRAVEL_Absolute);
	if (!BeaconURL.Valid || BeaconURL.Host.IsEmpty())
	{
		return false;
	}

	PendingRequestId = RequestId;
	if (!InitClient(BeaconURL))
	{
		PendingRequestId = 0;
		return false;
	}
	return true;
}

FNelaricTargetDecision& ANelaricTransitionBeaconClient::OnTargetDecision()
{
	return TargetDecision;
}

void ANelaricTransitionBeaconClient::OnConnected()
{
	if (PendingRequestId != 0)
	{
		ServerRequestTargetApproval(PendingRequestId);
	}
}

void ANelaricTransitionBeaconClient::OnFailure()
{
	if (!bDecisionDelivered && PendingRequestId != 0)
	{
		bDecisionDelivered = true;
		TargetDecision.Broadcast(PendingRequestId, false, false);
	}
	Super::OnFailure();
}

void ANelaricTransitionBeaconClient::ServerRequestTargetApproval_Implementation(uint64 RequestId)
{
	const ANelaricGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ANelaricGameModeBase>() : nullptr;
	ClientReceiveTargetDecision(RequestId, RequestId != 0 && GameMode && GameMode->CanAcceptTransition());
}

void ANelaricTransitionBeaconClient::ClientReceiveTargetDecision_Implementation(uint64 RequestId, bool bApproved)
{
	if (!bDecisionDelivered && RequestId == PendingRequestId)
	{
		bDecisionDelivered = true;
		TargetDecision.Broadcast(RequestId, bApproved, true);
	}
}
