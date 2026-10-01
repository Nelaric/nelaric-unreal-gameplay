// Copyright (c) 2026 Nelaric Contributors

#include "DemoGameMode.h"
#include "GAS/DemoGasPlayerState.h"
#include "Character/DemoPlayerCharacter.h"
#include "UObject/ConstructorHelpers.h"
ADemoGameMode::ADemoGameMode()
{
	PlayerStateClass = ADemoGasPlayerState::StaticClass();
	DefaultPawnClass = ADemoPlayerCharacter::StaticClass();
	static ConstructorHelpers::FClassFinder<ADemoPlayerCharacter> Pawn(
	    TEXT("/Game/Demo/Characters/BP_PlayerCharacter"));
	if (Pawn.Succeeded())
	{
		DefaultPawnClass = Pawn.Class;
	}
}
