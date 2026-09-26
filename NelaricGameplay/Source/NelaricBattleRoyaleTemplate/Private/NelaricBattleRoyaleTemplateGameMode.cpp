// Copyright (c) 2026 Nelaric

#include "NelaricBattleRoyaleTemplateGameMode.h"

#include "NelaricBattleRoyaleTemplatePlayerController.h"
#include "NelaricBattleRoyaleTemplatePlayerState.h"

ANelaricBattleRoyaleGameMode::ANelaricBattleRoyaleGameMode()
{
	PlayerControllerClass = ANelaricBattleRoyalePlayerController::StaticClass();
	PlayerStateClass = ANelaricBattleRoyalePlayerState::StaticClass();
}
