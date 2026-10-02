// Copyright (c) 2026 Nelaric Contributors

#include "Session/TransitionBeaconClient.h"

#include "Engine/EngineBaseTypes.h"
#include "NelaricGameModeBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricBeacon, Log, All);

bool ATransitionBeaconClient::BeginTargetApproval(const Nelaric::FNetworkEndpoint& Endpoint, uint64 RequestId)
{
	if (RequestId == 0 || !Endpoint.IsValid() || PendingRequestId != 0)
	{
		UE_LOG(
		    LogNelaricBeacon, Error,
		    TEXT("Target approval failed (request=%llu pending=%llu): invalid request, endpoint or pending request."),
		    RequestId, PendingRequestId);
		return false;
	}

	const FString AddressWithPort = FString::Printf(TEXT("%s:%d"), *Endpoint.Address, Endpoint.Port);
	FURL BeaconURL(nullptr, *AddressWithPort, TRAVEL_Absolute);
	if (!BeaconURL.Valid || BeaconURL.Host.IsEmpty())
	{
		UE_LOG(LogNelaricBeacon, Error,
		       TEXT("Target approval failed (request=%llu pending=%llu): malformed beacon URL."), RequestId,
		       PendingRequestId);
		return false;
	}

	PendingRequestId = RequestId;
	if (!InitClient(BeaconURL))
	{
		PendingRequestId = 0;
		UE_LOG(LogNelaricBeacon, Error,
		       TEXT("Target approval failed (request=%llu pending=%llu): beacon client initialization failed."),
		       RequestId, PendingRequestId);
		return false;
	}
	return true;
}

FTargetDecision& ATransitionBeaconClient::OnTargetDecision()
{
	return TargetDecision;
}

void ATransitionBeaconClient::OnConnected()
{
	if (PendingRequestId != 0)
	{
		ServerRequestTargetApproval(PendingRequestId);
	}
}

void ATransitionBeaconClient::OnFailure()
{
	if (!bDecisionDelivered && PendingRequestId != 0)
	{
		UE_LOG(LogNelaricBeacon, Error, TEXT("Target approval connection failed (request=%llu)."), PendingRequestId);
		bDecisionDelivered = true;
		TargetDecision.Broadcast(PendingRequestId, false, false);
	}
	Super::OnFailure();
}

void ATransitionBeaconClient::ServerRequestTargetApproval_Implementation(uint64 RequestId)
{
	const ANelaricGameModeBase* GameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ANelaricGameModeBase>() : nullptr;
	ClientReceiveTargetDecision(RequestId, RequestId != 0 && GameMode && GameMode->CanAcceptTransition());
}

void ATransitionBeaconClient::ClientReceiveTargetDecision_Implementation(uint64 RequestId, bool bApproved)
{
	if (!bDecisionDelivered && RequestId == PendingRequestId)
	{
		bDecisionDelivered = true;
		TargetDecision.Broadcast(RequestId, bApproved, true);
	}
}
