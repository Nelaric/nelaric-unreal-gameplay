// Copyright (c) 2026 Nelaric Contributors

/** @file DemoGasPlayerState.h Declares common demo participant state. */
#pragma once
#include "NelaricGasPlayerState.h"
#include "DemoGasPlayerState.generated.h"

/// Installs demo character attributes for both human and bot participants.
UCLASS(MinimalAPI, Blueprintable)
class ADemoGasPlayerState : public ANelaricGasPlayerState
{
	GENERATED_BODY()
public:
public:
	DEMOGAME_API ADemoGasPlayerState();
};
