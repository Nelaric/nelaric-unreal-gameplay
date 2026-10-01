// Copyright (c) 2026 Nelaric Contributors

#include "NelaricGasPlayerController.h"
#include "Player/ControlSwitchSubsystem.h"
#include "PawnGasBindingComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

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
	}
	Super::PawnLeavingGame();
}
