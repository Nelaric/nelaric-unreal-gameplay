// Copyright (c) 2026 Nelaric Contributors

#pragma once

#include "Pawn/NelaricPawnComponent.h"

#include "GameFramework/Controller.h"
#include "GameFramework/PlayerState.h"

#include "PawnComponentTestTypes.generated.h"

// Concrete fixtures for automation; no gameplay-facing API.
UCLASS(MinimalAPI, NotBlueprintable, Transient)
class UPawnComponentTestComponent final : public UNelaricPawnComponent
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI, NotBlueprintable, Transient)
class APawnComponentTestController final : public AController
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI, NotBlueprintable, Transient)
class APawnComponentTestPlayerState final : public APlayerState
{
	GENERATED_BODY()
};
