// Copyright (c) 2026 Nelaric

#pragma once

#include "Pawn/NelaricPawnComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

#include "NelaricPawnComponentTestTypes.generated.h"

// Concrete fixtures for automation; no gameplay-facing API.
UCLASS(MinimalAPI, NotBlueprintable, Transient)
class UNelaricPawnComponentTestComponent final : public UNelaricPawnComponent
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI, NotBlueprintable, Transient)
class ANelaricPawnComponentTestController final : public AController
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI, NotBlueprintable, Transient)
class ANelaricPawnComponentTestPlayerState final : public APlayerState
{
	GENERATED_BODY()
};
