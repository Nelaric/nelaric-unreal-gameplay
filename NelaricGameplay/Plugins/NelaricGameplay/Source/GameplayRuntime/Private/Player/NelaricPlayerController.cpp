// Copyright (c) 2026 Nelaric Contributors

#include "Player/NelaricPlayerController.h"

bool ANelaricPlayerController::RequestDepartureApproval(uint64 RequestId, const FString& TargetAddress)
{
	if (RequestId == 0 || TargetAddress.IsEmpty() || !IsLocalController())
	{
		return false;
	}

	ServerRequestDepartureApproval(RequestId, TargetAddress);
	return true;
}

FDepartureDecision& ANelaricPlayerController::OnDepartureDecision()
{
	return DepartureDecision;
}

void ANelaricPlayerController::ServerRequestDepartureApproval_Implementation(uint64 RequestId,
                                                                             const FString& TargetAddress)
{
	ClientReceiveDepartureDecision(RequestId, TargetAddress, RequestId != 0 && !TargetAddress.IsEmpty());
}

void ANelaricPlayerController::ClientReceiveDepartureDecision_Implementation(uint64 RequestId,
                                                                             const FString& TargetAddress,
                                                                             bool bApproved)
{
	DepartureDecision.Broadcast(RequestId, TargetAddress, bApproved);
}
