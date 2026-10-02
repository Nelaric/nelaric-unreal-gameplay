// Copyright (c) 2026 Nelaric Contributors

#include "DemoGameMode.h"
#include "GAS/DemoGasPlayerState.h"
#include "Character/DemoPlayerCharacter.h"

ADemoGameMode::ADemoGameMode()
{
	PlayerStateClass = ADemoGasPlayerState::StaticClass();
	// Concrete demo maps select their authored pawn in a GameMode Blueprint.
	DefaultPawnClass = ADemoPlayerCharacter::StaticClass();
}
