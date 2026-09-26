// Copyright (c) 2026 Nelaric

#include "NelaricMobaTemplateGameMode.h"

#include "NelaricMobaTemplatePlayerController.h"
#include "NelaricMobaTemplatePlayerState.h"

ANelaricMobaGameMode::ANelaricMobaGameMode()
{
	PlayerControllerClass = ANelaricMobaPlayerController::StaticClass();
	PlayerStateClass = ANelaricMobaPlayerState::StaticClass();
}
