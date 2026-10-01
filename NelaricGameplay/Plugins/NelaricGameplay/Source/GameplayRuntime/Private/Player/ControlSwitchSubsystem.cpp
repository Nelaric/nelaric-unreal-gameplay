// Copyright (c) 2026 Nelaric Contributors

#include "Player/ControlSwitchSubsystem.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
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

EControlSwitchResult UControlSwitchSubsystem::ValidateRequest(AController* Requester, EControlSwitchAction Action,
                                                              APawn* TargetPawn) const
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
	if (Requester->PlayerState->IsOnlyASpectator())
	{
		return EControlSwitchResult::Denied;
	}
	APawn* CurrentPawn = Requester->GetPawn();
	if (CurrentPawn && !Nelaric::Control::Matches(Requester, CurrentPawn, World))
	{
		return EControlSwitchResult::InvalidRequest;
	}
	if (Action == EControlSwitchAction::TakeControl)
	{
		if (!Nelaric::Control::IsLive(TargetPawn, World) ||
		    (World->GetNetMode() != NM_Standalone && !TargetPawn->GetIsReplicated()))
		{
			return EControlSwitchResult::InvalidTarget;
		}
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
	else if (Action != EControlSwitchAction::ReturnControl || TargetPawn)
	{
		return EControlSwitchResult::InvalidRequest;
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
		return EControlSwitchResult::Busy;
	}
	TGuardValue<bool> OperationGuard(bExecutingControlSwitch, true);
	EControlSwitchResult Result = ValidateRequest(Requester, Action, TargetPawn);
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
	if (!GameMode->CanChangePawnControl(Requester, Action, TargetPawn ? TargetPawn : OldPawn))
	{
		return EControlSwitchResult::Denied;
	}
	Result = ValidateRequest(Requester, Action, TargetPawn);
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
	// Spawning a replacement can run game callbacks; recheck before mutation.
	Result = ValidateRequest(Requester, Action, TargetPawn);
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
		if (Nelaric::Control::IsLive(Requester, World) && TargetPawn && Requester->GetPawn() == TargetPawn)
		{
			Requester->UnPossess();
		}
		if (Nelaric::Control::IsLive(ReturnController, World) && ReturnController->GetPawn() == OldPawn)
		{
			ReturnController->UnPossess();
		}
		const bool bTargetRestored =
		    !TargetPawn || Nelaric::Control::Restore(PreviousTargetController, TargetPawn, World);
		const bool bRequesterRestored = Nelaric::Control::Restore(Requester, OldPawn, World);
		return bTargetRestored && bRequesterRestored ? EControlSwitchResult::ExecutionFailed
		                                             : EControlSwitchResult::RecoveryFailed;
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
		Nelaric::Control::FContextChange RecoveryContext(OldPawn, TargetPawn);
		return Recover();
	}
	Requester->ForceNetUpdate();
	return EControlSwitchResult::Succeeded;
}
