// Copyright (c) 2026 Nelaric Contributors

#include "NelaricGasGameMode.h"
#include "NelaricGasPlayerState.h"
#include "NelaricGasPlayerController.h"
ANelaricGasGameMode::ANelaricGasGameMode()
{
	PlayerStateClass = ANelaricGasPlayerState::StaticClass();
	PlayerControllerClass = ANelaricGasPlayerController::StaticClass();
}
