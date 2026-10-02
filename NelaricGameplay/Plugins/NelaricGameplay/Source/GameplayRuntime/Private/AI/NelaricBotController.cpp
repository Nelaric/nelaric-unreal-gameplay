// Copyright (c) 2026 Nelaric Contributors

#include "AI/NelaricBotController.h"

#include "BrainComponent.h"

ANelaricBotController::ANelaricBotController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bWantsPlayerState = true;
	bStopAILogicOnUnposses = true;
}

void ANelaricBotController::OnUnPossess()
{
	StopMovement();
	if (BrainComponent)
	{
		BrainComponent->StopLogic(TEXT("Control handed to another participant"));
	}
	Super::OnUnPossess();
}
