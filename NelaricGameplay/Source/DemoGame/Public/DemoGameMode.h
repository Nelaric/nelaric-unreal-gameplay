// Copyright (c) 2026 Nelaric Contributors

/** @file DemoGameMode.h Declares the demo's GAS-enabled game mode. */
#pragma once
#include "NelaricGasGameMode.h"
#include "DemoGameMode.generated.h"

/// Selects shared demo participant state and the selectable demo character.
UCLASS(MinimalAPI, Blueprintable)
class ADemoGameMode : public ANelaricGasGameMode
{
	GENERATED_BODY()
public:
public:
	DEMOGAME_API ADemoGameMode();
};
