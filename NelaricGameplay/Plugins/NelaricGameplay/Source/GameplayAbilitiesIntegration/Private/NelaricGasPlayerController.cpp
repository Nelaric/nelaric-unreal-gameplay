// Copyright (c) 2026 Nelaric Contributors

#include "NelaricGasPlayerController.h"
#include "Player/ControlSwitchSubsystem.h"
#include "PawnGasBindingComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricGasPlayerControl, Log, All);

void ANelaricGasPlayerController::PawnLeavingGame()
{
	APawn* Previous = GetPawn();
	if (HasAuthority() && IsValid(Previous) && Previous->FindComponentByClass<UPawnGasBindingComponent>() &&
	    GetWorld() && !GetWorld()->bIsTearingDown)
	{
		auto* Coordinator = GetWorld()->GetSubsystem<UControlSwitchSubsystem>();
		if (Coordinator && Coordinator->ExecuteControlSwitch(this, EControlSwitchAction::ReturnControl, nullptr) ==
		                       EControlSwitchResult::Succeeded)
		{
			return;
		}
		if (!Coordinator)
		{
			UE_LOG(LogNelaricGasPlayerControl, Error,
			       TEXT("Cannot return GAS pawn control on departure: controller=%s pawn=%s; control coordinator is "
			            "unavailable."),
			       *GetName(), *GetNameSafe(Previous));
		}
	}
	Super::PawnLeavingGame();
}
