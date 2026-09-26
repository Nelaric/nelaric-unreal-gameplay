// Copyright (c) 2026 Nelaric

#include "NelaricOpenWorldTemplateGameMode.h"

#include "NelaricOpenWorldTemplatePlayerController.h"
#include "NelaricOpenWorldTemplatePlayerState.h"

ANelaricOpenWorldGameMode::ANelaricOpenWorldGameMode()
{
	PlayerControllerClass = ANelaricOpenWorldPlayerController::StaticClass();
	PlayerStateClass = ANelaricOpenWorldPlayerState::StaticClass();
}
