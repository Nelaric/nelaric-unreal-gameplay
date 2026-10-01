// Copyright (c) 2026 Nelaric Contributors

#include "Player/ControlSwitchSubsystem.h"

#include "Player/ControlSwitchPlan.h"
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

static bool HasPlayerState(const AController* Controller, const UWorld* World)
{
	return IsLive(Controller, World) && IsLive(Controller->PlayerState, World) &&
	       Controller->PlayerState->GetOwner() == Controller;
}

static bool Matches(const AController* Controller, const APawn* Pawn, const UWorld* World)
{
	return HasPlayerState(Controller, World) && IsLive(Pawn, World) && Controller->GetPawn() == Pawn &&
	       Pawn->GetController() == Controller && Pawn->GetPlayerState() == Controller->PlayerState;
}

static UPawnControlComponent* FindPolicy(const APawn* Pawn)
{
	if (!IsValid(Pawn))
	{
		return nullptr;
	}
	TInlineComponentArray<UPawnControlComponent*> Policies(Pawn);
	// More than one policy is ambiguous; never approve an arbitrary first one.
	return Policies.Num() == 1 ? Policies[0] : nullptr;
}

static bool IsPolicyReady(const UPawnControlComponent* Policy, const APawn* Pawn)
{
	return IsValid(Policy) && Policy->IsRegistered() && Policy->GetOwner() == Pawn &&
	       Policy->GetInitState() == EInitState::Ready && !Policy->HasTerminalInitFailure();
}

static FPolicySnapshot CapturePolicy(UPawnControlComponent* Policy)
{
	FPolicySnapshot Snapshot;
	if (Policy)
	{
		Snapshot.Component = Policy;
		Snapshot.Generation = Policy->GetInitGeneration();
		Snapshot.ReturnControllerClass = Policy->ReturnControllerClass.Get();
		Snapshot.bAllowPlayerControl = Policy->bAllowPlayerControl;
		Snapshot.bAllowReturnControl = Policy->bAllowReturnControl;
		Snapshot.bReturnToBot = Policy->bReturnToBot;
		Snapshot.bStartBotLogicOnReady = Policy->bStartBotLogicOnReady;
	}
	return Snapshot;
}

static bool MatchesPolicy(const FPolicySnapshot& Snapshot, const APawn* Pawn)
{
	const UPawnControlComponent* Policy = Snapshot.Component.Get();
	if (Snapshot.Component.IsExplicitlyNull())
	{
		return !Pawn;
	}
	return IsValid(Policy) && Policy == FindPolicy(Pawn) && Policy->IsRegistered() && Policy->GetOwner() == Pawn &&
	       Policy->GetInitGeneration() == Snapshot.Generation && Policy->GetInitState() == EInitState::Ready &&
	       !Policy->HasTerminalInitFailure() &&
	       Policy->ReturnControllerClass.Get() == Snapshot.ReturnControllerClass.Get() &&
	       Policy->bAllowPlayerControl == Snapshot.bAllowPlayerControl &&
	       Policy->bAllowReturnControl == Snapshot.bAllowReturnControl &&
	       Policy->bReturnToBot == Snapshot.bReturnToBot &&
	       Policy->bStartBotLogicOnReady == Snapshot.bStartBotLogicOnReady;
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
	bShuttingDown = true;
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

EControlSwitchResult
UControlSwitchSubsystem::ReserveParticipants(const TArray<TWeakObjectPtr<const AActor>>& Participants,
                                             const FGuid& TransitionId)
{
	const UWorld* World = GetWorld();
	if (bShuttingDown || !World || World->bIsTearingDown)
	{
		return EControlSwitchResult::WorldUnavailable;
	}
	check(TransitionId.IsValid());
	// This pass has no callbacks or writes: conflicts never reserve half a set.
	for (const auto& Participant : Participants)
	{
		if (!Nelaric::Control::IsLive(Participant.Get(), World))
		{
			return EControlSwitchResult::StaleRequest;
		}
		if (HasConflictingTransition(Participant.Get(), TransitionId))
		{
			return EControlSwitchResult::ControlTransitionInProgress;
		}
	}
	for (const auto& Participant : Participants)
	{
		ControlTransitions.Add(Participant, TransitionId);
	}
	return EControlSwitchResult::Succeeded;
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
	if (bShuttingDown || !World || World->bIsTearingDown)
	{
		return EControlSwitchResult::WorldUnavailable;
	}
	if (World->GetNetMode() == NM_Client || !Nelaric::Control::IsLive(Requester, World))
	{
		return EControlSwitchResult::InvalidRequest;
	}
	if (!Nelaric::Control::HasPlayerState(Requester, World))
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
	if (CurrentPawn && World->GetNetMode() != NM_Standalone && !CurrentPawn->GetIsReplicated())
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
			if (!Nelaric::Control::HasPlayerState(ExistingController, World))
			{
				return EControlSwitchResult::PlayerStateUnavailable;
			}
			if (!Nelaric::Control::Matches(ExistingController, TargetPawn, World))
			{
				return EControlSwitchResult::InvalidTarget;
			}
		}
		UPawnControlComponent* TargetPolicy = Nelaric::Control::FindPolicy(TargetPawn);
		if (!IsValid(TargetPolicy) || !TargetPolicy->IsRegistered() || !TargetPolicy->bAllowPlayerControl)
		{
			return EControlSwitchResult::Denied;
		}
		if (!Nelaric::Control::IsPolicyReady(TargetPolicy, TargetPawn))
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
		UPawnControlComponent* CurrentPolicy = Nelaric::Control::FindPolicy(CurrentPawn);
		if (!IsValid(CurrentPolicy) || !CurrentPolicy->IsRegistered() || !CurrentPolicy->bAllowReturnControl)
		{
			return EControlSwitchResult::Denied;
		}
		if (!Nelaric::Control::IsPolicyReady(CurrentPolicy, CurrentPawn))
		{
			return EControlSwitchResult::Busy;
		}
	}
	return EControlSwitchResult::Succeeded;
}

EControlSwitchResult UControlSwitchSubsystem::BuildPlan(AController* Requester, EControlSwitchAction Action,
                                                        APawn* TargetPawn, Nelaric::Control::FSwitchPlan& Plan) const
{
	const EControlSwitchResult Result = ValidateRequest(Requester, Action, TargetPawn, FGuid());
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	UWorld* World = GetWorld();
	ANelaricGameModeBase* GameMode = World->GetAuthGameMode<ANelaricGameModeBase>();
	if (!Nelaric::Control::IsLive(GameMode, World))
	{
		return EControlSwitchResult::NotHandled;
	}
	Plan.TransitionId = FGuid::NewGuid();
	Plan.Action = Action;
	Plan.GameMode = GameMode;
	Plan.PlayerStateClass = GameMode->PlayerStateClass.Get();
	Plan.Requester = Requester;
	Plan.RequesterState = Requester->PlayerState;
	Plan.OldPawn = Requester->GetPawn();
	Plan.TargetPawn = TargetPawn;
	Plan.TargetPawnState = TargetPawn ? TargetPawn->GetPlayerState() : nullptr;
	AController* PreviousController = TargetPawn ? TargetPawn->GetController() : nullptr;
	Plan.PreviousTargetController = PreviousController;
	Plan.PreviousTargetState = PreviousController ? PreviousController->PlayerState.Get() : nullptr;
	Plan.OldPolicy = Nelaric::Control::CapturePolicy(Nelaric::Control::FindPolicy(Plan.OldPawn.Get()));
	Plan.TargetPolicy = Nelaric::Control::CapturePolicy(Nelaric::Control::FindPolicy(TargetPawn));
	Plan.bNeedsReturnController =
	    Plan.OldPawn.IsValid() && Plan.OldPawn.Get() != TargetPawn && Plan.OldPolicy.bReturnToBot;
	if (Plan.bNeedsReturnController)
	{
		AAIController* ReturnController = Plan.OldPolicy.Component->FindReturnController();
		Plan.ReturnController = ReturnController;
		Plan.ReturnState = ReturnController ? ReturnController->PlayerState.Get() : nullptr;
		if (!ReturnController)
		{
			const UClass* ReturnClass = Plan.OldPolicy.ReturnControllerClass.Get();
			if (!IsValid(ReturnClass) || !ReturnClass->IsChildOf(AAIController::StaticClass()) ||
			    ReturnClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				return EControlSwitchResult::ReplacementUnavailable;
			}
		}
	}
	auto Include = [&Plan](const AActor* Actor)
	{
		if (Actor)
		{
			Plan.Participants.AddUnique(Actor);
		}
	};
	Include(Requester);
	Include(Plan.RequesterState.Get());
	Include(Plan.OldPawn.Get());
	Include(TargetPawn);
	Include(Plan.TargetPawnState.Get());
	Include(PreviousController);
	Include(Plan.PreviousTargetState.Get());
	Include(Plan.ReturnController.Get());
	Include(Plan.ReturnState.Get());
	return ValidatePlan(Plan, false);
}

EControlSwitchResult UControlSwitchSubsystem::ValidatePlan(const Nelaric::Control::FSwitchPlan& Plan,
                                                           bool bRequireReservation) const
{
	const UWorld* World = GetWorld();
	if (bShuttingDown || !World || World->bIsTearingDown)
	{
		return EControlSwitchResult::WorldUnavailable;
	}
	auto MatchesSnapshot = [&]()
	{
		for (const auto& Participant : Plan.Participants)
		{
			if (!Nelaric::Control::IsLive(Participant.Get(), World))
			{
				return false;
			}
		}
		const AController* Requester = Plan.Requester.Get();
		const APawn* TargetPawn = Plan.TargetPawn.Get();
		const ANelaricGameModeBase* GameMode = Plan.GameMode.Get();
		if (!Nelaric::Control::IsLive(GameMode, World) || World->GetAuthGameMode() != GameMode ||
		    GameMode->PlayerStateClass.Get() != Plan.PlayerStateClass.Get() ||
		    !Nelaric::Control::HasPlayerState(Requester, World) ||
		    Requester->PlayerState != Plan.RequesterState.Get() || Requester->GetPawn() != Plan.OldPawn.Get() ||
		    !Nelaric::Control::MatchesPolicy(Plan.OldPolicy, Plan.OldPawn.Get()) ||
		    !Nelaric::Control::MatchesPolicy(Plan.TargetPolicy, TargetPawn))
		{
			return false;
		}
		if (Plan.OldPawn.IsValid() && !Nelaric::Control::Matches(Requester, Plan.OldPawn.Get(), World))
		{
			return false;
		}
		if (TargetPawn && (TargetPawn->GetController() != Plan.PreviousTargetController.Get() ||
		                   TargetPawn->GetPlayerState() != Plan.TargetPawnState.Get()))
		{
			return false;
		}
		const AController* PreviousController = Plan.PreviousTargetController.Get();
		if (PreviousController && (PreviousController->PlayerState != Plan.PreviousTargetState.Get() ||
		                           !Nelaric::Control::Matches(PreviousController, TargetPawn, World)))
		{
			return false;
		}
		const AAIController* ReturnController = Plan.ReturnController.Get();
		return !ReturnController ||
		       (Nelaric::Control::HasPlayerState(ReturnController, World) &&
		        ReturnController->PlayerState == Plan.ReturnState.Get() && !ReturnController->GetPawn());
	};
	if (!MatchesSnapshot())
	{
		return EControlSwitchResult::StaleRequest;
	}
	for (const auto& Participant : Plan.Participants)
	{
		if (!Nelaric::Control::IsLive(Participant.Get(), World))
		{
			return EControlSwitchResult::StaleRequest;
		}
		const FGuid* ReservedId = ControlTransitions.Find(Participant);
		if ((ReservedId && *ReservedId != Plan.TransitionId) || (bRequireReservation && !ReservedId))
		{
			return EControlSwitchResult::ControlTransitionInProgress;
		}
	}
	EControlSwitchResult Result = ValidateRequest(Plan.Requester.Get(), Plan.Action, Plan.TargetPawn.Get(),
	                                              bRequireReservation ? Plan.TransitionId : FGuid());
	if (Result == EControlSwitchResult::Succeeded && bRequireReservation)
	{
		// Run even read-only gameplay predicates under the complete reservation.
		TArray<TWeakObjectPtr<UPawnControlComponent>> Policies;
		if (Plan.OldPolicy.Component.IsValid())
		{
			Policies.Add(Plan.OldPolicy.Component);
		}
		if (Plan.TargetPolicy.Component.IsValid())
		{
			Policies.AddUnique(Plan.TargetPolicy.Component);
		}
		for (const auto& Policy : Policies)
		{
			if (!Policy.IsValid())
			{
				return EControlSwitchResult::StaleRequest;
			}
			const bool bApplicable = Policy->IsInitApplicable();
			if (bShuttingDown || World->bIsTearingDown)
			{
				return EControlSwitchResult::WorldUnavailable;
			}
			if (!MatchesSnapshot())
			{
				return EControlSwitchResult::StaleRequest;
			}
			if (!bApplicable)
			{
				return EControlSwitchResult::Busy;
			}
		}
	}
	if (bShuttingDown || World->bIsTearingDown)
	{
		return EControlSwitchResult::WorldUnavailable;
	}
	// Applicability is a gameplay predicate too: catch context changes it makes.
	return MatchesSnapshot() ? Result : EControlSwitchResult::StaleRequest;
}

EControlSwitchResult UControlSwitchSubsystem::PreparePlan(Nelaric::Control::FSwitchPlan& Plan)
{
	EControlSwitchResult Result = ValidatePlan(Plan);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	ANelaricGameModeBase* GameMode = Plan.GameMode.Get();
	const bool bApproved = GameMode->CanChangePawnControl(
	    Plan.Requester.Get(), Plan.Action, Plan.TargetPawn.IsValid() ? Plan.TargetPawn.Get() : Plan.OldPawn.Get());
	Result = ValidatePlan(Plan);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	if (!bApproved)
	{
		return EControlSwitchResult::Denied;
	}
	if (!Plan.bNeedsReturnController || Plan.ReturnController.IsValid())
	{
		return EControlSwitchResult::Succeeded;
	}
	UWorld* World = GetWorld();
	APawn* OldPawn = Plan.OldPawn.Get();
	UClass* ReturnClass = Plan.OldPolicy.ReturnControllerClass.Get();
	if (!IsValid(ReturnClass) ||
	    ReturnClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return EControlSwitchResult::ReplacementUnavailable;
	}
	const FTransform SpawnTransform(OldPawn->GetActorRotation(), OldPawn->GetActorLocation());
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Parameters.OverrideLevel = OldPawn->GetLevel();
	Parameters.ObjectFlags |= RF_Transient;
	Parameters.bDeferConstruction = true;
	EControlSwitchResult SpawnReservation = EControlSwitchResult::ReplacementUnavailable;
	Parameters.CustomPreSpawnInitalization = [&](AActor* Actor)
	{
		AAIController* Controller = CastChecked<AAIController>(Actor);
		Plan.SpawnedReturnController = Controller;
		// Reserve before construction, spawn delegates and BeginPlay can reenter.
		SpawnReservation = ReserveParticipants({Controller}, Plan.TransitionId);
		if (SpawnReservation == EControlSwitchResult::Succeeded)
		{
			Plan.Participants.AddUnique(Controller);
			if (UPawnControlComponent* Policy = Plan.OldPolicy.Component.Get())
			{
				Policy->TrackSpawnedController(Controller);
			}
		}
	};
	AAIController* Spawned = World->SpawnActor<AAIController>(ReturnClass, SpawnTransform, Parameters);
	if (SpawnReservation != EControlSwitchResult::Succeeded)
	{
		return SpawnReservation;
	}
	Result = ValidatePlan(Plan);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	if (!Nelaric::Control::IsLive(Spawned, World) || Spawned->GetPawn())
	{
		return EControlSwitchResult::ReplacementUnavailable;
	}
	Spawned->FinishSpawning(SpawnTransform);
	// FinishSpawning initializes PlayerState and may execute arbitrary game code.
	Result = ValidatePlan(Plan);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	Spawned = Plan.SpawnedReturnController.Get();
	if (!Nelaric::Control::HasPlayerState(Spawned, World) || Spawned->GetPawn())
	{
		return EControlSwitchResult::ReplacementUnavailable;
	}
	Plan.ReturnController = Spawned;
	Plan.ReturnState = Spawned->PlayerState;
	Result = ReserveParticipants({Spawned, Spawned->PlayerState.Get()}, Plan.TransitionId);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	Plan.Participants.AddUnique(Plan.ReturnState.Get());
	return ValidatePlan(Plan);
}

void UControlSwitchSubsystem::DiscardPreparedController(const Nelaric::Control::FSwitchPlan& Plan)
{
	AAIController* Controller = Plan.SpawnedReturnController.Get();
	if (!Nelaric::Control::IsLive(Controller, GetWorld()) || Controller->GetPawn())
	{
		return;
	}
	if (UPawnControlComponent* Policy = Plan.OldPolicy.Component.Get())
	{
		Policy->ForgetSpawnedController(Controller);
	}
	// Leave reused bots and controllers adopted by game code alone.
	Controller->Destroy();
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
	Nelaric::Control::FSwitchPlan Plan;
	EControlSwitchResult Result = BuildPlan(Requester, Action, TargetPawn, Plan);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	const FGuid TransitionId = Plan.TransitionId;
	ON_SCOPE_EXIT
	{
		DiscardPreparedController(Plan);
		if (!RecoveryRequiredTransitions.Contains(TransitionId))
		{
			ReleaseTransition(TransitionId);
		}
	};
	Result = ReserveParticipants(Plan.Participants, TransitionId);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	Result = PreparePlan(Plan);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	Result = ValidatePlan(Plan);
	if (Result != EControlSwitchResult::Succeeded)
	{
		return Result;
	}
	UWorld* World = GetWorld();
	APawn* OldPawn = Plan.OldPawn.Get();
	AController* PreviousTargetController = Plan.PreviousTargetController.Get();
	AAIController* ReturnController = Plan.ReturnController.Get();
	if (OldPawn == TargetPawn)
	{
		return EControlSwitchResult::Succeeded;
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
	// Ready callbacks can destroy actors or attempt direct possession.
	ContextChange.Finish();
	if ((TargetPawn && !Nelaric::Control::Matches(Requester, TargetPawn, World)) ||
	    (!TargetPawn && (!Nelaric::Control::IsLive(Requester, World) || Requester->GetPawn())) ||
	    (ReturnController && !Nelaric::Control::Matches(ReturnController, OldPawn, World)) ||
	    (OldPawn && !ReturnController && (!Nelaric::Control::IsLive(OldPawn, World) || OldPawn->GetController())))
	{
		return Recover();
	}
	if (UPawnControlComponent* OldPolicy = Plan.OldPolicy.Component.Get(); OldPolicy && ReturnController)
	{
		OldPolicy->RememberController(ReturnController);
	}
	if (UPawnControlComponent* TargetPolicy = Plan.TargetPolicy.Component.Get())
	{
		TargetPolicy->RememberController(Cast<AAIController>(PreviousTargetController));
	}
	Requester->ForceNetUpdate();
	return EControlSwitchResult::Succeeded;
}
