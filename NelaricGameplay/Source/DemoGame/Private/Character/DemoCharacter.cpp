// Copyright (c) 2026 Nelaric Contributors

#include "Character/DemoCharacter.h"

#include "AI/NelaricBotController.h"
#include "Pawn/PawnControlComponent.h"

ADemoCharacter::ADemoCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PawnControlComponent = CreateDefaultSubobject<UPawnControlComponent>(TEXT("PawnControlComponent"));
	AIControllerClass = ANelaricBotController::StaticClass();
}
