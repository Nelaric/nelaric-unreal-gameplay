// Copyright (c) 2026 Nelaric Contributors

#include "Player/NelaricPlayerController.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Player/ControlSwitchSubsystem.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricPlayerControl, Log, All);

int32 ANelaricPlayerController::RequestTakeControl(APawn* TargetPawn)
{
	check(IsInGameThread());
	if (!IsValid(TargetPawn) || TargetPawn->IsActorBeingDestroyed() || TargetPawn->GetWorld() != GetWorld())
	{
		UE_LOG(LogNelaricPlayerControl, Error,
		       TEXT("Cannot request pawn control on %s: invalid target %s or target belongs to another world."),
		       *GetName(), *GetNameSafe(TargetPawn));
		return 0;
	}
	return SendControlSwitchRequest(EControlSwitchAction::TakeControl, TargetPawn);
}

int32 ANelaricPlayerController::RequestReturnControl()
{
	check(IsInGameThread());
	return SendControlSwitchRequest(EControlSwitchAction::ReturnControl, nullptr);
}

FControlSwitchDecision& ANelaricPlayerController::OnControlSwitchDecision()
{
	return ControlSwitchDecision;
}

int32 ANelaricPlayerController::SendControlSwitchRequest(EControlSwitchAction Action, APawn* TargetPawn)
{
	const UWorld* World = GetWorld();
	if (!IsLocalPlayerController() || IsActorBeingDestroyed() || !World || World->bIsTearingDown ||
	    NextControlRequestId == 0)
	{
		UE_LOG(LogNelaricPlayerControl, Error,
		       TEXT("Cannot send control request on %s: localPlayer=%d destroying=%d world=%s nextRequest=%d."),
		       *GetName(), IsLocalPlayerController(), IsActorBeingDestroyed(), *GetNameSafe(World),
		       NextControlRequestId);
		return 0;
	}
	const int32 RequestId = NextControlRequestId;
	NextControlRequestId = RequestId == MAX_int32 ? 0 : RequestId + 1;
	ServerRequestControlSwitch(RequestId, Action, TargetPawn, GetPawn());
	return RequestId;
}

void ANelaricPlayerController::ServerRequestControlSwitch_Implementation(int32 RequestId, EControlSwitchAction Action,
                                                                         APawn* TargetPawn, APawn* ExpectedCurrentPawn)
{
	check(IsInGameThread());
	const EControlSwitchResult Result =
	    EvaluateControlSwitchRequest(RequestId, Action, TargetPawn, ExpectedCurrentPawn);
	if (Result != EControlSwitchResult::Succeeded && Result != EControlSwitchResult::Busy &&
	    Result != EControlSwitchResult::ControlTransitionInProgress)
	{
		UE_LOG(LogNelaricPlayerControl, Error,
		       TEXT("Authority control request failed: controller=%s request=%d action=%s target=%s result=%s."),
		       *GetName(), RequestId, *UEnum::GetValueAsString(Action), *GetNameSafe(TargetPawn),
		       *UEnum::GetValueAsString(Result));
	}
	ClientReceiveControlSwitchDecision(RequestId, Action, TargetPawn, Result);
}

EControlSwitchResult ANelaricPlayerController::EvaluateControlSwitchRequest(int32 RequestId,
                                                                            EControlSwitchAction Action,
                                                                            APawn* TargetPawn,
                                                                            APawn* ExpectedCurrentPawn)
{
	const UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown || IsActorBeingDestroyed())
	{
		return EControlSwitchResult::WorldUnavailable;
	}
	if (!HasAuthority() || World->GetNetMode() == NM_Client || RequestId <= 0)
	{
		return EControlSwitchResult::InvalidRequest;
	}
	if (RequestId <= LastAuthorityControlRequestId)
	{
		return EControlSwitchResult::StaleRequest;
	}
	LastAuthorityControlRequestId = RequestId;
	if (ExpectedCurrentPawn != GetPawn())
	{
		return EControlSwitchResult::StaleRequest;
	}
	if (Action == EControlSwitchAction::TakeControl)
	{
		if (!IsValid(TargetPawn) || TargetPawn->IsActorBeingDestroyed() || TargetPawn->GetWorld() != World ||
		    !TargetPawn->HasAuthority())
		{
			return EControlSwitchResult::InvalidTarget;
		}
	}
	else if (Action != EControlSwitchAction::ReturnControl || TargetPawn != nullptr)
	{
		return EControlSwitchResult::InvalidRequest;
	}
	const UControlSwitchSubsystem* Coordinator = World->GetSubsystem<UControlSwitchSubsystem>();
	if (Coordinator &&
	    (Coordinator->IsControlTransitionInProgress(this) || Coordinator->IsControlTransitionInProgress(PlayerState) ||
	     Coordinator->IsControlTransitionInProgress(GetPawn()) ||
	     Coordinator->IsControlTransitionInProgress(TargetPawn)))
	{
		return EControlSwitchResult::ControlTransitionInProgress;
	}
	if (bHandlingControlRequest)
	{
		return EControlSwitchResult::Busy;
	}

	TGuardValue<bool> RequestGuard(bHandlingControlRequest, true);
	return HandleControlSwitchRequest(Action, TargetPawn);
}

EControlSwitchResult ANelaricPlayerController::HandleControlSwitchRequest_Implementation(EControlSwitchAction Action,
                                                                                         APawn* TargetPawn)
{
	UWorld* World = GetWorld();
	UControlSwitchSubsystem* Coordinator = World ? World->GetSubsystem<UControlSwitchSubsystem>() : nullptr;
	return Coordinator ? Coordinator->ExecuteControlSwitch(this, Action, TargetPawn) : EControlSwitchResult::NotHandled;
}

void ANelaricPlayerController::ClientReceiveControlSwitchDecision_Implementation(int32 RequestId,
                                                                                 EControlSwitchAction Action,
                                                                                 APawn* TargetPawn,
                                                                                 EControlSwitchResult Result)
{
	ControlSwitchDecision.Broadcast(RequestId, Action, TargetPawn, Result);
}

bool ANelaricPlayerController::RequestDepartureApproval(uint64 RequestId, const FString& TargetAddress)
{
	if (RequestId == 0 || TargetAddress.IsEmpty() || !IsLocalController())
	{
		UE_LOG(LogNelaricPlayerControl, Error,
		       TEXT("Cannot request departure approval on %s (request=%llu): invalid request, empty target address or "
		            "non-local controller."),
		       *GetName(), RequestId);
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
