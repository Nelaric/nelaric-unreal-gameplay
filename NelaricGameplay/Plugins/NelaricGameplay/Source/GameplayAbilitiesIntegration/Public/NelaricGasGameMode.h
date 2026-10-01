// Copyright (c) 2026 Nelaric Contributors

/** @file NelaricGasGameMode.h Declares an opt-in GAS game mode. */
#pragma once
#include "NelaricGameModeBase.h"
#include "NelaricGasGameMode.generated.h"

/// Selects the common GAS PlayerState for human and bot participants.
UCLASS(MinimalAPI, Blueprintable)
class ANelaricGasGameMode : public ANelaricGameModeBase
{
	GENERATED_BODY()
public:
public:
	GAMEPLAYABILITIESINTEGRATION_API ANelaricGasGameMode();
};
