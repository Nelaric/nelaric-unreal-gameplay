// Copyright (c) 2026 Nelaric Contributors

/** @file NelaricGasPlayerController.h Declares participant departure cleanup. */
#pragma once
#include "Player/NelaricPlayerController.h"
#include "NelaricGasPlayerController.generated.h"

/// Hands a selectable character back before connection cleanup destroys it.
UCLASS(MinimalAPI, Blueprintable)
class ANelaricGasPlayerController : public ANelaricPlayerController
{
	GENERATED_BODY()
public:
public:
	GAMEPLAYABILITIESINTEGRATION_API virtual void PawnLeavingGame() override;
};
