// Copyright (c) 2026 Nelaric Contributors

#include "Player/ControlSwitchSubsystem.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Misc/ScopeExit.h"
#include "NelaricGameModeBase.h"
#include "Pawn/PawnControlComponent.h"
#include "Pawn/PawnInitializationComponent.h"
#include "Templates/UnrealTemplate.h"

namespace Nelaric::Control
{
static bool IsLive(const AActor* Actor, const UWorld* World)
{
	return IsValid(Actor) && !Actor->IsActorBeingDestroyed() && Actor->GetWorld() == World && Actor->HasAuthority();
}

static bool Matches(const AController* Controller, const APawn* Pawn, const UWorld* World)
{
	return IsLive(Controller, World) && IsLive(Pawn, World) && Controller->GetPawn() == Pawn &&
	       Pawn->GetController() == Controller && Pawn->GetPlayerState() == Controller->PlayerState;
}

static bool Restore(AController* Controller, APawn* Pawn, UWorld* World)
{
	if (!Pawn)
	{
		return IsLive(Controller, World) && !Controller->GetPawn();
	}
	if (!Controller)
	{
		return IsLive(Pawn, World) && !Pawn->GetController();
	}
	if (Matches(Controller, Pawn, World))
	{
		return true;
	}
	if (!IsLive(Controller, World) || !IsLive(Pawn, World) || Controller->GetPawn() || Pawn->GetController() ||
	    World->bIsTearingDown)
	{
		return false;
	}
	Controller->Possess(Pawn);
	return Matches(Controller, Pawn, World);
}

struct FContextChange
{
	TArray<TWeakObjectPtr<UPawnInitializationComponent>> Components;

	explicit FContextChange(APawn* First, APawn* Second)
	{
		for (APawn* Pawn : {First, Second})
		{
			if (IsValid(Pawn))
			{
				if (UPawnInitializationComponent* Component =
				        Pawn->FindComponentByClass<UPawnInitializationComponent>())
				{
					Components.AddUnique(Component);
				}
			}
		}
		for (const auto& Component : Components)
		{
			if (Component.IsValid())
			{
				Component->BeginPawnContextChange();
			}
		}
	}

	~FContextChange()
	{
		Finish();
	}

	void Finish()
	{
		const auto EndingComponents = MoveTemp(Components);
		for (const auto& Component : EndingComponents)
		{
			if (Component.IsValid())
			{
				Component->EndPawnContextChange();
			}
		}
	}
};

static void StopContext(AController* Controller, APawn* Pawn)
{
	if (IsValid(Controller))
	{
		Controller->StopMovement();
	}
	for (AActor* Owner : {static_cast<AActor*>(Controller), static_cast<AActor*>(Pawn)})
	{
		if (IsValid(Owner))
		{
			TInlineComponentArray<UBrainComponent*> Brains(Owner);
			for (UBrainComponent* Brain : Brains)
			{
				if (IsValid(Brain) && Brain->IsRunning())
				{
					Brain->StopLogic(TEXT("Pawn control changing"));
				}
			}
		}
	}
}
} // namespace Nelaric::Control

bool UControlSwitchSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && World->GetNetMode() != NM_Client;
}

void UControlSwitchSubsystem::Deinitialize()
{
	ControlTransitions.Reset();
	RecoveryRequiredTransitions.Reset();
	Super::Deinitialize();
}

bool UControlSwitchSubsystem::IsControlTransitionInProgress(const AActor* Actor) const
{
	check(IsInGameThread());
	return Nelaric::Control::IsLive(Actor, GetWorld()) && ControlTransitions.Contains(Actor);
}

bool UControlSwitchSubsystem::IsControlTransitionRecoveryRequired(const AActor* Actor) const
{
	check(IsInGameThread());
	if (!Nelaric::Control::IsLive(Actor, GetWorld()))
	{
		return false;
	}
	const FGuid* TransitionId = ControlTransitions.Find(Actor);
	return TransitionId && RecoveryRequiredTransitions.Contains(*TransitionId);
}

bool UControlSwitchSubsystem::ResolveControlTransitionRecovery(const AActor* Actor)
{
	check(IsInGameThread());
	UWorld* World = GetWorld();
	if (bExecutingControlSwitch || !World || World->bIsTearingDown || !Nelaric::Control::IsLive(Actor, World))
	{
		return false;
	}
	PruneDestroyedParticipants();
	const FGuid* FoundId = ControlTransitions.Find(Actor);
	if (!FoundId || !RecoveryRequiredTransitions.Contains(*FoundId))
	{
		return false;
	}
	const FGuid TransitionId = *FoundId;
	TGuardValue<bool> OperationGuard(bExecutingControlSwitch, true);
	TArray<TWeakObjectPtr<const AActor>> Participants;
	for (const auto& Entry : ControlTransitions)
	{
		if (Entry.Value == TransitionId)
		{
			Participants.Add(Entry.Key);
		}
	}
	// Repairs may settle into new relationships; never force possession here.
	for (const auto& WeakParticipant : Participants)
	{
		const AActor* Participant = WeakParticipant.Get();
		if (!Nelaric::Control::IsLive(Participant, World))
		{
			return false;
		}
		if (const APawn* ReservedPawn = Cast<APawn>(Participant))
		{
			const AController* Controller = ReservedPawn->GetController();
			if (Controller && (!Nelaric::Control::Matches(Controller, ReservedPawn, World) ||
			                   !Nelaric::Control::IsLive(Controller->PlayerState, World) ||
			                   HasConflictingTransition(Controller, TransitionId) ||
			                   HasConflictingTransition(Controller->PlayerState, TransitionId)))
			{
				return false;
			}
			const UPawnControlComponent* Policy = ReservedPawn->FindComponentByClass<UPawnControlComponent>();
			if (!IsValid(Policy) || !Policy->IsRegistered() || Policy->GetInitState() != Nelaric::EInitState::Ready ||
			    Policy->HasTerminalInitFailure() || !Policy->IsInitApplicable())
			{
				return false;
			}
		}
		else if (const AController* ReservedController = Cast<AController>(Participant))
		{
			const APawn* Pawn = ReservedController->GetPawn();
			if (!Nelaric::Control::IsLive(ReservedController->PlayerState, World) ||
			    HasConflictingTransition(ReservedController->PlayerState, TransitionId) ||
			    (Pawn && (!Nelaric::Control::Matches(ReservedController, Pawn, World) ||
			              HasConflictingTransition(Pawn, TransitionId))))
			{
				return false;
			}
		}
		else if (const APlayerState* PlayerState = Cast<APlayerState>(Participant))
		{
			const AController* Controller = Cast<AController>(PlayerState->GetOwner());
			if (!Nelaric::Control::IsLive(Controller, World) || Controller->PlayerState != PlayerState ||
			    HasConflictingTransition(Controller, TransitionId))
			{
				return false;
			}
		}
	}
	if (World->bIsTearingDown || !RecoveryRequiredTransitions.Contains(TransitionId))
	{
		return false;
	}
	ReleaseTransition(TransitionId);
	return true;
}

bool UControlSwitchSubsystem::HasConflictingTransition(const AActor* Actor, const FGuid& TransitionId) const
{
	if (!IsValid(Actor))
	{
		return false;
	}
	const FGuid* ExistingId = ControlTransitions.Find(Actor);
	return ExistingId && *ExistingId != TransitionId;
}

bool UControlSwitchSubsystem::ReserveController(AController* Controller, const FGuid& TransitionId)
{
	if (!Nelaric::Control::IsLive(Controller, GetWorld()) ||
	    !Nelaric::Control::IsLive(Controller->PlayerState, GetWorld()) ||
	    HasConflictingTransition(Controller, TransitionId) ||
	    HasConflictingTransition(Controller->PlayerState, TransitionId))
	{
		return false;
	}
	ControlTransitions.Add(Controller, TransitionId);
	ControlTransitions.Add(Controller->PlayerState, TransitionId);
	return true;
}

void UControlSwitchSubsystem::ReleaseTransition(const FGuid& TransitionId)
{
	for (auto It = ControlTransitions.CreateIterator(); It; ++It)
	{
		if (It.Value() == TransitionId)
		{
			It.RemoveCurrent();
		}
	}
	RecoveryRequiredTransitions.Remove(TransitionId);
}

void UControlSwitchSubsystem::PruneDestroyedParticipants()
{
	TSet<FGuid> SurvivingTransitions;
	for (auto It = ControlTransitions.CreateIterator(); It; ++It)
	{
		if (!Nelaric::Control::IsLive(It.Key().Get(), GetWorld()))
		{
			It.RemoveCurrent();
		}
		else
		{
			SurvivingTransitions.Add(It.Value());
		}
	}
	for (auto It = RecoveryRequiredTransitions.CreateIterator(); It; ++It)
	{
		if (!SurvivingTransitions.Contains(*It))
		{
			It.RemoveCurrent();
		}
	}
}

EControlSwitchResult UControlSwitchSubsystem::ValidateRequest(AController* Requester, EControlSwitchAction Action,
                                                              APawn* TargetPawn, const FGuid& TransitionId) const
{
	const UWorld* World = GetWorld();
	if (!World || World->bIsTearingDown)
	{
		return EControlSwitchResult::WorldUnavailable;
	}
	if (World->GetNetMode() == NM_Client || !Nelaric::Control::IsLive(Requester, World))
	{
		return EControlSwitchResult::InvalidRequest;
	}
	if (!Nelaric::Control::IsLive(Requester->PlayerState, World))
	{
		return EControlSwitchResult::PlayerStateUnavailable;
	}
	if (Action == EControlSwitchAction::TakeControl)
	{
		if (!Nelaric::Control::IsLive(TargetPawn, World) ||
		    (World->GetNetMode() != NM_Standalone && !TargetPawn->GetIsReplicated()))
		{
			return EControlSwitchResult::InvalidTarget;
		}
	}
	else if (Action != EControlSwitchAction::ReturnControl || TargetPawn)
	{
		return EControlSwitchResult::InvalidRequest;
	}
	APawn* CurrentPawn = Requester->GetPawn();
	AController* TargetController = TargetPawn ? TargetPawn->GetController() : nullptr;
	if (HasConflictingTransition(Requester, TransitionId) ||
	    HasConflictingTransition(Requester->PlayerState, TransitionId) ||
	    HasConflictingTransition(CurrentPawn, TransitionId) || HasConflictingTransition(TargetPawn, TransitionId) ||
	    HasConflictingTransition(TargetController, TransitionId) ||
	    (IsValid(TargetController) && HasConflictingTransition(TargetController->PlayerState, TransitionId)))
	{
		return EControlSwitchResult::ControlTransitionInProgress;
	}
	if (Requester->PlayerState->IsOnlyASpectator())
	{
		return EControlSwitchResult::Denied;
	}
	if (CurrentPawn && !Nelaric::Control::Matches(Requester, CurrentPawn, World))
	{
		return EControlSwitchResult::InvalidRequest;
	}
	if (Action == EControlSwitchAction::TakeControl)
	{
		AController* ExistingController = TargetPawn->GetController();
		if (ExistingController && ExistingController != Requester)
		{
			if (!Nelaric::Control::IsLive(ExistingController, World) || ExistingController->GetPawn() != TargetPawn)
			{
				return EControlSwitchResult::InvalidTarget;
			}
			if (ExistingController->IsPlayerController())
			{
				return EControlSwitchResult::TargetOccupied;
			}
			if (!Cast<AAIController>(ExistingController))
			{
				return EControlSwitchResult::Denied;
			}
			if (!Nelaric::Control::IsLive(ExistingController->PlayerState, World))
			{
				return EControlSwitchResult::PlayerStateUnavailable;
			}
		}
		UPawnControlComponent* TargetPolicy = TargetPawn->FindComponentByClass<UPawnControlComponent>();
		if (!IsValid(TargetPolicy) || !TargetPolicy->IsRegistered() || !TargetPolicy->bAllowPlayerControl)
		{
			return EControlSwitchResult::Denied;
		}
		if (TargetPolicy->GetInitState() != Nelaric::EInitState::Ready || TargetPolicy->HasTerminalInitFailure() ||
		    !TargetPolicy->IsInitApplicable())
		{
			return EControlSwitchResult::Busy;
		}
	}
	else if (!CurrentPawn)
	{
		return EControlSwitchResult::NoCurrentPawn;
	}
	if (CurrentPawn && CurrentPawn != TargetPawn)
	{
		UPawnControlComponent* CurrentPolicy = CurrentPawn->FindComponentByClass<UPawnControlComponent>();
		if (!IsValid(CurrentPolicy) || !CurrentPolicy->IsRegistered() || !CurrentPolicy->bAllowReturnControl)
		{
			return EControlSwitchResult::Denied;
		}
		if (CurrentPolicy->GetInitState() != Nelaric::EInitState::Ready || CurrentPolicy->HasTerminalInitFailure() ||
		    !CurrentPolicy->IsInitApplicable())
		{
			return EControlSwitchResult::Busy;
		}
	}
	return EControlSwitchResult::Succeeded;
}

EControlSwitchResult UControlSwitchSubsystem::ExecuteControlSwitch(AController* Requester, EControlSwitchAction Action,
                                                                   APawn* TargetPawn)
{
	check(IsInGameThread());
	if (bExecutingControlSwitch)
	{
		if (IsControlTransitionInProgress(Requester) || IsControlTransitionInProgress(TargetPawn) ||
		    (IsValid(Requester) && (IsControlTransitionInProgress(Requester->PlayerState) ||
		                            IsControlTransitionInProgress(Requester->GetPawn()))) ||
		    (IsValid(TargetPawn) && IsControlTransitionInProgress(TargetPawn->GetController())))
		{
			return EControlSwitchResult::ControlTransitionInProgress;
		}
		return EControlSwitchResult::Busy;
	}
	TGuardValue<bool> OperationGuard(bExecutingControlSwitch, true);
	PruneDestroyedParticipants();
	EControlSwitchResult Result = ValidateRequest(Requester, Action, TargetPawn, FGuid());
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	UWorld* World = GetWorld();
	ANelaricGameModeBase* GameMode = World->GetAuthGameMode<ANelaricGameModeBase>();
	if (!IsValid(GameMode))
	{
		return EControlSwitchResult::NotHandled;
	}
	APawn* OldPawn = Requester->GetPawn();
	AController* PreviousTargetController = TargetPawn ? TargetPawn->GetController() : nullptr;
	const FGuid TransitionId = FGuid::NewGuid();
	ON_SCOPE_EXIT
	{
		if (!RecoveryRequiredTransitions.Contains(TransitionId))
		{
			ReleaseTransition(TransitionId);
		}
	};
	if (!ReserveController(Requester, TransitionId) ||
	    (PreviousTargetController && !ReserveController(PreviousTargetController, TransitionId)))
	{
		return EControlSwitchResult::ControlTransitionInProgress;
	}
	if (OldPawn)
	{
		ControlTransitions.Add(OldPawn, TransitionId);
	}
	if (TargetPawn)
	{
		ControlTransitions.Add(TargetPawn, TransitionId);
	}
	if (!GameMode->CanChangePawnControl(Requester, Action, TargetPawn ? TargetPawn : OldPawn))
	{
		return EControlSwitchResult::Denied;
	}
	Result = ValidateRequest(Requester, Action, TargetPawn, TransitionId);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	if (Requester->GetPawn() != OldPawn || (TargetPawn && TargetPawn->GetController() != PreviousTargetController))
	{
		return EControlSwitchResult::StaleRequest;
	}
	if (OldPawn == TargetPawn)
	{
		return EControlSwitchResult::Succeeded;
	}
	UPawnControlComponent* OldPolicy = OldPawn ? OldPawn->FindComponentByClass<UPawnControlComponent>() : nullptr;
	const bool bReturnToBot = OldPolicy && OldPolicy->bReturnToBot;
	AAIController* ReturnController = bReturnToBot ? OldPolicy->PrepareReturnController() : nullptr;
	if (bReturnToBot && !ReturnController)
	{
		return EControlSwitchResult::ReplacementUnavailable;
	}
	if (ReturnController && !ReserveController(ReturnController, TransitionId))
	{
		return EControlSwitchResult::ControlTransitionInProgress;
	}
	// Spawning a replacement can run game callbacks; recheck before mutation.
	Result = ValidateRequest(Requester, Action, TargetPawn, TransitionId);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	if (Requester->GetPawn() != OldPawn || (TargetPawn && TargetPawn->GetController() != PreviousTargetController) ||
	    (ReturnController && ReturnController->GetPawn()))
	{
		return EControlSwitchResult::StaleRequest;
	}

	Nelaric::Control::FContextChange ContextChange(OldPawn, TargetPawn);
	Nelaric::Control::StopContext(Requester, OldPawn);
	if (PreviousTargetController)
	{
		Nelaric::Control::StopContext(PreviousTargetController, TargetPawn);
	}
	auto Recover = [&]()
	{
		Nelaric::Control::FContextChange RecoveryContext(OldPawn, TargetPawn);
		if (Nelaric::Control::IsLive(Requester, World) && TargetPawn && Requester->GetPawn() == TargetPawn)
		{
			Requester->UnPossess();
		}
		if (Nelaric::Control::IsLive(ReturnController, World) && ReturnController->GetPawn() == OldPawn)
		{
			ReturnController->UnPossess();
		}
		Nelaric::Control::Restore(PreviousTargetController, TargetPawn, World);
		Nelaric::Control::Restore(Requester, OldPawn, World);
		RecoveryContext.Finish();
		ContextChange.Finish();
		// Ready callbacks may change relationships after Restore returned.
		const bool bTargetRestored =
		    !TargetPawn ||
		    (PreviousTargetController ? Nelaric::Control::Matches(PreviousTargetController, TargetPawn, World)
		                              : Nelaric::Control::IsLive(TargetPawn, World) && !TargetPawn->GetController());
		const bool bRequesterRestored = OldPawn ? Nelaric::Control::Matches(Requester, OldPawn, World)
		                                        : Nelaric::Control::IsLive(Requester, World) && !Requester->GetPawn();
		if (!bTargetRestored || !bRequesterRestored ||
		    (ReturnController && (!Nelaric::Control::IsLive(ReturnController, World) || ReturnController->GetPawn())))
		{
			RecoveryRequiredTransitions.Add(TransitionId);
			return EControlSwitchResult::RecoveryFailed;
		}
		return EControlSwitchResult::ExecutionFailed;
	};
	if (World->bIsTearingDown || !Nelaric::Control::IsLive(Requester, World) || Requester->GetPawn() != OldPawn ||
	    (OldPawn && !Nelaric::Control::Matches(Requester, OldPawn, World)) ||
	    (TargetPawn &&
	     (!Nelaric::Control::IsLive(TargetPawn, World) || TargetPawn->GetController() != PreviousTargetController)))
	{
		return Recover();
	}
	if (OldPawn)
	{
		Requester->UnPossess();
	}
	if (!Nelaric::Control::IsLive(Requester, World) || Requester->GetPawn() ||
	    (OldPawn && (!Nelaric::Control::IsLive(OldPawn, World) || OldPawn->GetController())))
	{
		return Recover();
	}
	if (TargetPawn)
	{
		if (!Nelaric::Control::IsLive(TargetPawn, World) || TargetPawn->GetController() != PreviousTargetController)
		{
			return Recover();
		}
		if (PreviousTargetController)
		{
			if (!Nelaric::Control::IsLive(PreviousTargetController, World))
			{
				return Recover();
			}
			PreviousTargetController->UnPossess();
		}
		if (!Nelaric::Control::IsLive(TargetPawn, World) || TargetPawn->GetController() ||
		    !Nelaric::Control::IsLive(Requester, World) || Requester->GetPawn())
		{
			return Recover();
		}
		Requester->Possess(TargetPawn);
		if (!Nelaric::Control::Matches(Requester, TargetPawn, World))
		{
			return Recover();
		}
	}
	if (ReturnController)
	{
		if (!Nelaric::Control::IsLive(ReturnController, World) || ReturnController->GetPawn() ||
		    !Nelaric::Control::IsLive(OldPawn, World) || OldPawn->GetController())
		{
			return Recover();
		}
		ReturnController->Possess(OldPawn);
		if (!Nelaric::Control::Matches(ReturnController, OldPawn, World))
		{
			return Recover();
		}
	}
	if ((TargetPawn && !Nelaric::Control::Matches(Requester, TargetPawn, World)) ||
	    (!TargetPawn && (!Nelaric::Control::IsLive(Requester, World) || Requester->GetPawn())))
	{
		return Recover();
	}
	if (TargetPawn && IsValid(TargetPawn))
	{
		if (UPawnControlComponent* TargetPolicy = TargetPawn->FindComponentByClass<UPawnControlComponent>())
		{
			TargetPolicy->RememberController(Cast<AAIController>(PreviousTargetController));
		}
	}
	// Ready callbacks can destroy actors or attempt direct possession.
	ContextChange.Finish();
	if ((TargetPawn && !Nelaric::Control::Matches(Requester, TargetPawn, World)) ||
	    (!TargetPawn && (!Nelaric::Control::IsLive(Requester, World) || Requester->GetPawn())) ||
	    (ReturnController && !Nelaric::Control::Matches(ReturnController, OldPawn, World)) ||
	    (OldPawn && !ReturnController && (!Nelaric::Control::IsLive(OldPawn, World) || OldPawn->GetController())))
	{
		return Recover();
	}
	Requester->ForceNetUpdate();
	return EControlSwitchResult::Succeeded;
}
