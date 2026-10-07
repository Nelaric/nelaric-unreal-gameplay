// Copyright (c) 2026 Nelaric Contributors

#include "Player/DemoPlayerController.h"
#include "AI/DemoCompanyCommandActor.h"
#include "AI/DemoCompanyRegistrySubsystem.h"

#include "DemoControlGameMode.h"
#include "Character/DemoCharacter.h"
#include "Equipment/DemoEquipmentInstance.h"
#include "Equipment/DemoEquipmentManagerComponent.h"
#include "Player/DemoOverviewPawn.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Input/PlayerInputComponent.h"
#include "Pawn/PawnControlComponent.h"
#include "PawnGasBindingComponent.h"
#include "Player/ControlSwitchSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoControl, Log, All);

ADemoPlayerController::ADemoPlayerController()
{
	bAutoManageActiveCameraTarget = false;
}

bool ADemoPlayerController::RequestRifleActive(bool bActive)
{
	if (!IsInGameThread())
	{
		return false;
	}
	const ADemoCharacter* ControlledCharacter = Cast<ADemoCharacter>(GetPawn());
	if (bEndingPlay || IsActorBeingDestroyed() || !IsLocalPlayerController() ||
	    ControlMode != EDemoControlMode::ControllingCharacter || !IsValid(ControlledCharacter) ||
	    ControlledCharacter->IsActorBeingDestroyed() || ControlledCharacter->GetController() != this)
	{
		return false;
	}
	ServerSetRifleActive(bActive);
	return true;
}

void ADemoPlayerController::ServerSetRifleActive_Implementation(bool bActive)
{
	if (!HasAuthority() || bEndingPlay || IsActorBeingDestroyed() || !GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}
	// Resolve possession on authority; clients send only the desired state.
	ADemoCharacter* ControlledCharacter = Cast<ADemoCharacter>(GetPawn());
	UDemoEquipmentManagerComponent* Manager =
	    IsValid(ControlledCharacter) && !ControlledCharacter->IsActorBeingDestroyed() &&
	            ControlledCharacter->GetController() == this
	        ? ControlledCharacter->FindComponentByClass<UDemoEquipmentManagerComponent>()
	        : nullptr;
	if (!IsValid(Manager))
	{
		ReportRifleResult(bActive, EDemoEquipmentResult::NotReady);
		return;
	}
	if (!bActive)
	{
		ReportRifleResult(false, Manager->DeactivateEquipment());
		return;
	}
	UDemoEquipmentInstance* PrimaryWeapon = Manager->FindEquipmentInSlot(TEXT("PrimaryWeapon"));
	ReportRifleResult(true, IsValid(PrimaryWeapon) ? Manager->ActivateEquipment(PrimaryWeapon->GetEquipmentId())
	                                               : EDemoEquipmentResult::NotFound);
}

void ADemoPlayerController::ReportRifleResult(bool bActive, EDemoEquipmentResult Result)
{
	UE_LOG(LogDemoControl, Log, TEXT("Rifle selection authority: controller=%s pawn=%s active=%d result=%s."),
	       *GetName(), *GetNameSafe(GetPawn()), bActive, *UEnum::GetValueAsString(Result));
	ClientReportRifleResult(bActive, Result);
}

void ADemoPlayerController::ClientReportRifleResult_Implementation(bool bActive, EDemoEquipmentResult Result)
{
	if (bEndingPlay || IsActorBeingDestroyed())
	{
		return;
	}
	UE_LOG(LogDemoControl, Log, TEXT("Rifle selection client: controller=%s active=%d result=%s."), *GetName(), bActive,
	       *UEnum::GetValueAsString(Result));
	OnRifleActiveResult(bActive, Result);
}

APawn* ADemoPlayerController::GetSelectedBot() const
{
	return SelectedBot.Get();
}

ADemoOverviewPawn* ADemoPlayerController::GetOverviewPawn() const
{
	if (IsValid(OverviewPawn) && !OverviewPawn->IsActorBeingDestroyed())
	{
		return OverviewPawn;
	}
	return Cast<ADemoOverviewPawn>(GetPawn());
}

UCameraComponent* ADemoPlayerController::GetOverviewCamera() const
{
	const ADemoOverviewPawn* Overview = GetOverviewPawn();
	return IsValid(Overview) && !Overview->IsActorBeingDestroyed() ? Overview->GetCameraComponent() : nullptr;
}

void ADemoPlayerController::BeginPlay()
{
	Super::BeginPlay();
	InitializeLocalDemo();
}

void ADemoPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();
	if (HasActorBegunPlay())
	{
		InitializeLocalDemo();
	}
}

void ADemoPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	InitializeLocalDemo();
	if (bLocalInitialized && !bEndingPlay && IsLocalPlayerController())
	{
		UpdateLocalControl();
	}
}

void ADemoPlayerController::TickActor(float DeltaTime, ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);
	// Remote server controllers do not run PlayerTick without local input.
	RestoreAuthorityOverview();
}

void ADemoPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (GetPawn() == InPawn)
	{
		bRestoreOverviewRequested = false;
		if (ADemoOverviewPawn* Overview = Cast<ADemoOverviewPawn>(InPawn))
		{
			if (IsValid(OverviewPawn) && OverviewPawn != Overview && !OverviewPawn->GetController())
			{
				OverviewPawn->Destroy();
			}
			OverviewPawn = Overview;
			ForceNetUpdate();
		}
	}
}

void ADemoPlayerController::OnUnPossess()
{
	const bool bHadPawn = GetPawn() != nullptr;
	Super::OnUnPossess();
	if (HasAuthority() && bHadPawn && !bEndingPlay)
	{
		bRestoreOverviewRequested = true;
	}
}

void ADemoPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ADemoPlayerController, OverviewPawn, COND_OwnerOnly);
}

void ADemoPlayerController::InitializeLocalDemo()
{
	if (bLocalInitialized || bEndingPlay || !IsLocalPlayerController() || !HasActorBegunPlay() || !GetWorld() ||
	    GetWorld()->bIsTearingDown || !IsValid(PlayerState) || !IsValid(GetPawn()))
	{
		return;
	}
	DecisionHandle = OnControlSwitchDecision().AddUObject(this, &ThisClass::HandleControlDecision);
	bLocalInitialized = true;
	ReconcilePossession();
}

bool ADemoPlayerController::EnsureAuthorityOverviewPawn()
{
	if (IsValid(OverviewPawn) && !OverviewPawn->IsActorBeingDestroyed())
	{
		return OverviewPawn->GetOwner() == this;
	}
	UWorld* World = GetWorld();
	ADemoControlGameMode* Mode = World ? World->GetAuthGameMode<ADemoControlGameMode>() : nullptr;
	if (!HasAuthority() || bEndingPlay || !Mode || World->bIsTearingDown)
	{
		return false;
	}
	APawn* SpawnedPawn = Mode->SpawnDefaultPawnAtTransform(this, FTransform(GetSpawnLocation()));
	OverviewPawn = Cast<ADemoOverviewPawn>(SpawnedPawn);
	if (!IsValid(OverviewPawn) || OverviewPawn->IsActorBeingDestroyed())
	{
		if (IsValid(SpawnedPawn))
		{
			SpawnedPawn->Destroy();
		}
		OverviewPawn = nullptr;
		return false;
	}
	OverviewPawn->SetOwner(this);
	ForceNetUpdate();
	return true;
}

void ADemoPlayerController::RestoreAuthorityOverview()
{
	UWorld* World = GetWorld();
	if (!bRestoreOverviewRequested || !HasAuthority() || bEndingPlay || !World || World->bIsTearingDown || GetPawn() ||
	    !IsValid(PlayerState) || PlayerState->IsOnlyASpectator() || !IsInState(NAME_Playing))
	{
		return;
	}
	UControlSwitchSubsystem* Coordinator = World->GetSubsystem<UControlSwitchSubsystem>();
	if (!Coordinator || Coordinator->IsControlTransitionInProgress(this) ||
	    Coordinator->IsControlTransitionInProgress(PlayerState))
	{
		return;
	}
	if (!EnsureAuthorityOverviewPawn())
	{
		bRestoreOverviewRequested = false;
		UE_LOG(LogDemoControl, Error, TEXT("Could not restore the overview pawn for %s."), *GetName());
		return;
	}
	const UPawnControlComponent* Policy = OverviewPawn->GetControlPolicy();
	if (!IsValid(Policy) || Policy->GetInitState() != Nelaric::EInitState::Ready)
	{
		return;
	}
	const EControlSwitchResult Result =
	    Coordinator->ExecuteControlSwitch(this, EControlSwitchAction::TakeControl, OverviewPawn);
	if (Result != EControlSwitchResult::Busy && Result != EControlSwitchResult::ControlTransitionInProgress)
	{
		bRestoreOverviewRequested = false;
		if (Result != EControlSwitchResult::Succeeded)
		{
			UE_LOG(LogDemoControl, Error, TEXT("Overview restoration failed for %s: %s."), *GetName(),
			       *UEnum::GetValueAsString(Result));
		}
	}
}

EControlSwitchResult ADemoPlayerController::HandleControlSwitchRequest_Implementation(EControlSwitchAction Action,
                                                                                      APawn* TargetPawn)
{
	if (Action != EControlSwitchAction::ReturnControl)
	{
		return Super::HandleControlSwitchRequest_Implementation(Action, TargetPawn);
	}
	if (!HasAuthority() || !GetWorld() || GetWorld()->bIsTearingDown || bEndingPlay)
	{
		return EControlSwitchResult::WorldUnavailable;
	}
	if (!GetPawn() || GetPawn()->IsA<ADemoOverviewPawn>())
	{
		return EControlSwitchResult::NoCurrentPawn;
	}
	if (!EnsureAuthorityOverviewPawn())
	{
		return EControlSwitchResult::ReplacementUnavailable;
	}
	UControlSwitchSubsystem* Coordinator = GetWorld()->GetSubsystem<UControlSwitchSubsystem>();
	// A single coordinated switch releases the character, transfers GAS and
	// possesses the camera pawn. The transport still reports ReturnControl.
	return Coordinator ? Coordinator->ExecuteControlSwitch(this, EControlSwitchAction::TakeControl, OverviewPawn)
	                   : EControlSwitchResult::NotHandled;
}

void ADemoPlayerController::SetMode(EDemoControlMode NewMode)
{
	if (bEndingPlay || !GetWorld() || GetWorld()->bIsTearingDown)
	{
		return;
	}
	const EDemoControlMode PreviousMode = ControlMode;
	const bool bInitialPresentation = !bPresentationApplied;
	ControlMode = NewMode;
	const bool bCharacterView =
	    NewMode == EDemoControlMode::ControllingCharacter || NewMode == EDemoControlMode::ReturningControl;
	if (bCharacterView)
	{
		APawn* ControlledPawn = GetPawn();
		if (IsValid(ControlledPawn) && (GetViewTarget() != ControlledPawn || !bPresentationApplied))
		{
			SetViewTargetWithBlend(ControlledPawn, FMath::Max(0.0f, CameraBlendTime), VTBlend_Cubic, 0.0f, true);
		}
	}
	else
	{
		ADemoOverviewPawn* Overview = GetOverviewPawn();
		const bool bHasCamera = IsValid(Overview) && !Overview->IsActorBeingDestroyed() && GetOverviewCamera();
		if (bHasCamera && (GetViewTarget() != Overview || !bPresentationApplied))
		{
			const float Duration = bPresentationApplied ? FMath::Max(0.0f, CameraBlendTime) : 0.0f;
			SetViewTargetWithBlend(Overview, Duration, VTBlend_Cubic, 0.0f, true);
			bCameraUnavailableReported = false;
		}
		if (!bHasCamera && !bCameraUnavailableReported)
		{
			bCameraUnavailableReported = true;
			UE_LOG(LogDemoControl, Error, TEXT("No overview camera is available for %s."), *GetName());
			OnOverviewCameraUnavailable();
		}
	}
	bPresentationApplied = true;
	if (!bEndingPlay && (PreviousMode != NewMode || bInitialPresentation))
	{
		UE_LOG(LogDemoControl, Verbose, TEXT("Local control mode: %s"), *UEnum::GetValueAsString(NewMode));
		OnDemoModeChanged(PreviousMode, NewMode);
	}
}

void ADemoPlayerController::SetSelectedBot(APawn* NewBot)
{
	APawn* PreviousBot = SelectedBot.Get();
	if (PreviousBot != NewBot)
	{
		SelectedBot = NewBot;
		OnSelectedTargetChanged(PreviousBot, NewBot);
	}
}

bool ADemoPlayerController::IsSelectableBot(APawn* ControlledPawn) const
{
	if (!IsValid(ControlledPawn) || ControlledPawn->IsActorBeingDestroyed() ||
	    ControlledPawn->GetWorld() != GetWorld() || !ControlledPawn->IsA<ADemoCharacter>() ||
	    ControlledPawn->IsPlayerControlled())
	{
		return false;
	}
	if (!static_cast<const ADemoCharacter*>(ControlledPawn)->IsPoolActive())
	{
		return false;
	}
	const UPawnControlComponent* Policy = ControlledPawn->FindComponentByClass<UPawnControlComponent>();
	return Policy && Policy->bAllowPlayerControl;
}

bool ADemoPlayerController::TakeControlOfBot(APawn* TargetPawn)
{
	InitializeLocalDemo();
	if (!bLocalInitialized || bEndingPlay || !IsLocalPlayerController() || bSendingRequest || bPendingRequest ||
	    ControlMode != EDemoControlMode::Overview || !IsOverviewReady(GetPawn()))
	{
		UE_LOG(LogDemoControl, Error,
		       TEXT("Cannot take bot control on %s: localInitialized=%d pending=%d mode=%s; controller or overview "
		            "input is not ready."),
		       *GetName(), bLocalInitialized, bPendingRequest, *UEnum::GetValueAsString(ControlMode));
		return false;
	}
	if (!IsSelectableBot(TargetPawn))
	{
		UE_LOG(LogDemoControl, Error,
		       TEXT("Cannot take bot control: controller=%s target=%s; target is not selectable."), *GetName(),
		       *GetNameSafe(TargetPawn));
		OnControlRequestFailed(EControlSwitchAction::TakeControl, EControlSwitchResult::InvalidTarget);
		return false;
	}
	return BeginControlRequest(EControlSwitchAction::TakeControl, TargetPawn);
}

bool ADemoPlayerController::ReturnToOverview()
{
	if (!bLocalInitialized || bEndingPlay || !IsLocalPlayerController() || bSendingRequest || !IsValid(GetPawn()) ||
	    !GetPawn()->IsA<ADemoCharacter>())
	{
		UE_LOG(LogDemoControl, Error,
		       TEXT("Cannot return to overview on %s: localInitialized=%d pending=%d mode=%s; controller or character "
		            "is not ready."),
		       *GetName(), bLocalInitialized, bPendingRequest, *UEnum::GetValueAsString(ControlMode));
		return false;
	}
	if (bPendingRequest)
	{
		// An approved possession may lack configured character input. Returning
		// remains available without cancelling unresolved authority work.
		if (!bHasDecision || PendingDecision != EControlSwitchResult::Succeeded ||
		    PendingAction != EControlSwitchAction::TakeControl || GetPawn() != PendingTarget.Get())
		{
			return false;
		}
		ClearPendingRequest();
	}
	return BeginControlRequest(EControlSwitchAction::ReturnControl, nullptr);
}

bool ADemoPlayerController::BeginControlRequest(EControlSwitchAction Action, APawn* Target)
{
	TGuardValue<bool> SendingGuard(bSendingRequest, true);
	bPendingRequest = true;
	bHasDecision = false;
	bWaitTimeoutReported = false;
	PendingRequestId = 0;
	PendingAction = Action;
	PendingTarget = Target;
	WaitStartedAt = GetWorld()->GetRealTimeSeconds();
	if (Action == EControlSwitchAction::TakeControl)
	{
		SetSelectedBot(Target);
	}
	if (bEndingPlay || IsActorBeingDestroyed() || GetWorld()->bIsTearingDown)
	{
		ClearPendingRequest();
		return false;
	}
	SetMode(Action == EControlSwitchAction::TakeControl ? EDemoControlMode::TakingControl
	                                                    : EDemoControlMode::ReturningControl);
	if (bEndingPlay || IsActorBeingDestroyed() || GetWorld()->bIsTearingDown)
	{
		ClearPendingRequest();
		return false;
	}
	const int32 RequestId =
	    Action == EControlSwitchAction::TakeControl ? RequestTakeControl(Target) : RequestReturnControl();
	if (RequestId == 0)
	{
		UE_LOG(LogDemoControl, Error, TEXT("Could not send control request: controller=%s action=%s target=%s."),
		       *GetName(), *UEnum::GetValueAsString(Action), *GetNameSafe(Target));
		ClearPendingRequest();
		ReconcilePossession();
		OnControlRequestFailed(Action, EControlSwitchResult::InvalidRequest);
		return false;
	}
	// A standalone decision can arrive inside RequestTakeControl, before it
	// returns its ID. The handler accepts that ID only during this send.
	PendingRequestId = RequestId;
	return true;
}

void ADemoPlayerController::HandleControlDecision(int32 RequestId, EControlSwitchAction Action, APawn* Target,
                                                  EControlSwitchResult Result)
{
	if (bEndingPlay || !GetWorld() || GetWorld()->bIsTearingDown || !bPendingRequest || bHasDecision ||
	    Action != PendingAction || (PendingRequestId != RequestId && !(bSendingRequest && PendingRequestId == 0)))
	{
		return;
	}
	PendingRequestId = RequestId;
	PendingDecision = Result;
	bHasDecision = true;
	if (Result != EControlSwitchResult::Succeeded)
	{
		UE_LOG(LogDemoControl, Error,
		       TEXT("Control request failed: controller=%s request=%d action=%s target=%s result=%s."), *GetName(),
		       RequestId, *UEnum::GetValueAsString(Action), *GetNameSafe(Target), *UEnum::GetValueAsString(Result));
	}
	else
	{
		UE_LOG(LogDemoControl, Verbose, TEXT("Control request %d: %s"), RequestId, *UEnum::GetValueAsString(Result));
	}
	WaitStartedAt = GetWorld()->GetRealTimeSeconds();
	bWaitTimeoutReported = false;
}

void ADemoPlayerController::ClearPendingRequest()
{
	bPendingRequest = false;
	bHasDecision = false;
	PendingRequestId = 0;
	PendingTarget.Reset();
	bWaitTimeoutReported = false;
}

bool ADemoPlayerController::IsOverviewReady(APawn* ControlledPawn) const
{
	const ADemoOverviewPawn* Overview = Cast<ADemoOverviewPawn>(ControlledPawn);
	const UPawnControlComponent* Policy = IsValid(Overview) ? Overview->GetControlPolicy() : nullptr;
	return IsValid(Overview) && !Overview->IsActorBeingDestroyed() && Overview == GetOverviewPawn() &&
	       Overview->GetController() == this && IsValid(PlayerState) && Overview->GetPlayerState() == PlayerState &&
	       IsPawnInputReady(ControlledPawn) && IsValid(Policy) &&
	       Policy->GetInitState() == Nelaric::EInitState::Ready && IsValid(Overview->GetCameraComponent()) &&
	       Overview->GetCameraComponent()->IsActive();
}

bool ADemoPlayerController::IsCharacterReady(APawn* ControlledPawn) const
{
	if (!IsValid(ControlledPawn) || ControlledPawn->IsActorBeingDestroyed() ||
	    ControlledPawn->GetController() != this || !IsValid(PlayerState) ||
	    ControlledPawn->GetPlayerState() != PlayerState)
	{
		return false;
	}
	const UPawnGasBindingComponent* Binding = ControlledPawn->FindComponentByClass<UPawnGasBindingComponent>();
	if (!Binding || !Binding->IsReadyForActions())
	{
		return false;
	}
	return IsPawnInputReady(ControlledPawn);
}

bool ADemoPlayerController::IsPawnInputReady(APawn* ControlledPawn) const
{
	if (!IsValid(ControlledPawn) || !ControlledPawn->InputComponent)
	{
		return false;
	}
	TInlineComponentArray<UPlayerInputComponent*> Inputs(ControlledPawn);
	for (const UPlayerInputComponent* Input : Inputs)
	{
		if (Input->InputConfig)
		{
			if (!Input->GetBoundInputComponent() || Input->GetBoundInputComponent() != ControlledPawn->InputComponent)
			{
				return false;
			}
		}
	}
	// An absent input configuration intentionally supplies no native bindings.
	return true;
}

void ADemoPlayerController::ReportWaitTimeout()
{
	if (!bWaitTimeoutReported &&
	    GetWorld()->GetRealTimeSeconds() - WaitStartedAt >= FMath::Max(0.1f, ControlWaitTimeout))
	{
		bWaitTimeoutReported = true;
		UE_LOG(LogDemoControl, Error, TEXT("Control wait timed out for %s; reconciling without resending."),
		       *GetName());
		OnControlWaitTimedOut(PendingAction, bPendingRequest && !bHasDecision);
	}
}

void ADemoPlayerController::UpdateLocalControl()
{
	if (bSendingRequest)
	{
		return;
	}
	if (!bPendingRequest)
	{
		ReconcilePossession();
		return;
	}
	if (!bHasDecision)
	{
		ReportWaitTimeout();
		return;
	}
	if (PendingDecision != EControlSwitchResult::Succeeded)
	{
		const EControlSwitchAction Action = PendingAction;
		const EControlSwitchResult Result = PendingDecision;
		ClearPendingRequest();
		ReconcilePossession();
		OnControlRequestFailed(Action, Result);
		return;
	}
	if (PendingAction == EControlSwitchAction::ReturnControl && IsOverviewReady(GetPawn()))
	{
		ClearPendingRequest();
		PresentedPawn.Reset();
		SetSelectedBot(nullptr);
		SetMode(EDemoControlMode::Overview);
		return;
	}
	if (PendingAction == EControlSwitchAction::TakeControl)
	{
		APawn* Target = PendingTarget.Get();
		if (!IsValid(Target) || Target->IsActorBeingDestroyed())
		{
			UE_LOG(LogDemoControl, Error, TEXT("Control target disappeared while waiting: controller=%s request=%d."),
			       *GetName(), PendingRequestId);
			ClearPendingRequest();
			ReconcilePossession();
			OnControlRequestFailed(EControlSwitchAction::TakeControl, EControlSwitchResult::InvalidTarget);
			return;
		}
		if (GetPawn() == Target && IsCharacterReady(Target))
		{
			ClearPendingRequest();
			PresentedPawn = Target;
			SetMode(EDemoControlMode::ControllingCharacter);
			if (!bEndingPlay && !bPendingRequest && ControlMode == EDemoControlMode::ControllingCharacter &&
			    GetPawn() == Target && IsValid(Target) && !Target->IsActorBeingDestroyed())
			{
				OnControlledCharacterReady(Target);
			}
			return;
		}
	}
	ReportWaitTimeout();
}

void ADemoPlayerController::ReconcilePossession()
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn) || ControlledPawn->IsActorBeingDestroyed())
	{
		PresentedPawn.Reset();
		SetSelectedBot(nullptr);
		if (ControlMode != EDemoControlMode::Overview || !bPresentationApplied || GetViewTarget() != GetOverviewPawn())
		{
			SetMode(EDemoControlMode::Overview);
		}
		return;
	}
	if (ControlledPawn->IsA<ADemoOverviewPawn>())
	{
		PresentedPawn.Reset();
		SetSelectedBot(nullptr);
		if (ControlMode != EDemoControlMode::Overview || !bPresentationApplied || GetViewTarget() != ControlledPawn)
		{
			SetMode(EDemoControlMode::Overview);
		}
		return;
	}
	if (!IsCharacterReady(ControlledPawn))
	{
		PresentedPawn.Reset();
		if (ControlMode != EDemoControlMode::TakingControl)
		{
			PendingAction = EControlSwitchAction::TakeControl;
			WaitStartedAt = GetWorld()->GetRealTimeSeconds();
			bWaitTimeoutReported = false;
			SetMode(EDemoControlMode::TakingControl);
		}
		ReportWaitTimeout();
		return;
	}
	if (PresentedPawn.Get() != ControlledPawn || ControlMode != EDemoControlMode::ControllingCharacter)
	{
		PresentedPawn = ControlledPawn;
		SetMode(EDemoControlMode::ControllingCharacter);
		if (!bEndingPlay && !bPendingRequest && ControlMode == EDemoControlMode::ControllingCharacter &&
		    GetPawn() == ControlledPawn && IsValid(ControlledPawn) && !ControlledPawn->IsActorBeingDestroyed())
		{
			OnControlledCharacterReady(ControlledPawn);
		}
	}
}

void ADemoPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	OnControlSwitchDecision().Remove(DecisionHandle);
	ClearPendingRequest();
	if (HasAuthority() && IsValid(OverviewPawn))
	{
		OverviewPawn->Destroy();
	}
	OverviewPawn = nullptr;
	SelectedBot.Reset();
	PresentedPawn.Reset();
	Super::EndPlay(EndPlayReason);
}

namespace Nelaric::CompanyPlayer
{
static ADemoCompanyCommandActor* AuthorizedCompany(ADemoPlayerController* Controller, const FString& PlatoonId = {},
                                                   bool bReadOnly = false)
{
	auto* Registry = UDemoCommandLibrary::GetRegistry(Controller);
	auto* Company = Registry ? Cast<ADemoCompanyCommandActor>(Registry->GetCompany()) : nullptr;
	if (!Controller->HasAuthority() || !Controller->bCanCommandCompany || !Company || !Company->Definition ||
	    Company->Definition->TeamId != Controller->CompanyCommandTeamId ||
	    (!PlatoonId.IsEmpty() && !Company->Definition->ExpectedPlatoonIds.Contains(PlatoonId)) ||
	    (!bReadOnly && !Controller->CompanyPlatoonScope.IsEmpty() &&
	     (PlatoonId.IsEmpty() || !Controller->CompanyPlatoonScope.Contains(PlatoonId))))
		return nullptr;
	return Company;
}
} // namespace Nelaric::CompanyPlayer

bool ADemoPlayerController::RequestCompanyMission(const FDemoCompanyMission& Mission)
{
	if (!IsLocalController() || IsActorBeingDestroyed() || Mission.Objectives.Num() > 64)
		return false;
	ServerCompanyMission(Mission);
	return true;
}
bool ADemoPlayerController::RequestPlatoonManualScope(const FString& PlatoonId, bool bLocked)
{
	if (!IsLocalController() || IsActorBeingDestroyed() || PlatoonId.Len() > 128)
		return false;
	ServerCompanyScope(PlatoonId, bLocked);
	return true;
}
bool ADemoPlayerController::RequestManualPlatoonMission(const FString& PlatoonId, const FDemoPlatoonMission& Mission)
{
	if (!IsLocalController() || IsActorBeingDestroyed() || PlatoonId.Len() > 128)
		return false;
	ServerManualPlatoonMission(PlatoonId, Mission);
	return true;
}
bool ADemoPlayerController::RequestCompanySnapshot()
{
	if (!IsLocalController() || IsActorBeingDestroyed())
		return false;
	ServerCompanySnapshot();
	return true;
}
void ADemoPlayerController::ServerCompanyMission_Implementation(const FDemoCompanyMission& Mission)
{
	auto* Company = Nelaric::CompanyPlayer::AuthorizedCompany(this);
	const bool bAccepted = Company && Mission.Objectives.Num() <= 64 && Company->SubmitCompanyMission(Mission);
	ClientCompanyResult(bAccepted, Company ? Company->GetCompanyContext()->GetState() : FString());
}
void ADemoPlayerController::ServerCompanyScope_Implementation(const FString& PlatoonId, bool bLocked)
{
	auto* Company = Nelaric::CompanyPlayer::AuthorizedCompany(this, PlatoonId);
	const bool bAccepted = Company && PlatoonId.Len() <= 128 && Company->SetPlatoonManualScope(PlatoonId, bLocked);
	ClientCompanyResult(bAccepted, Company ? Company->GetCompanyContext()->GetState() : FString());
}
void ADemoPlayerController::ServerManualPlatoonMission_Implementation(const FString& PlatoonId,
                                                                      const FDemoPlatoonMission& Mission)
{
	auto* Company = Nelaric::CompanyPlayer::AuthorizedCompany(this, PlatoonId);
	const bool bAccepted = Company && PlatoonId.Len() <= 128 && Company->SubmitManualPlatoonMission(PlatoonId, Mission);
	ClientCompanyResult(bAccepted, Company ? Company->GetCompanyContext()->GetState() : FString());
}
void ADemoPlayerController::ServerCompanySnapshot_Implementation()
{
	auto* Company = Nelaric::CompanyPlayer::AuthorizedCompany(this, {}, true);
	ClientCompanyResult(Company != nullptr, Company ? Company->GetCompanyContext()->GetState() : FString());
}
void ADemoPlayerController::ClientCompanyResult_Implementation(bool bAccepted, const FString& Snapshot)
{
	OnCompanyCommandResult(bAccepted, Snapshot);
}
