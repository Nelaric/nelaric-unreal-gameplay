// Copyright (c) 2026 Nelaric

#include "NelaricSandboxTemplateGameMode.h"

#include "NelaricSandboxTemplatePlayerController.h"
#include "NelaricSandboxTemplatePlayerState.h"

ANelaricSandboxGameMode::ANelaricSandboxGameMode()
{
	PlayerControllerClass = ANelaricSandboxPlayerController::StaticClass();
	PlayerStateClass = ANelaricSandboxPlayerState::StaticClass();
}
