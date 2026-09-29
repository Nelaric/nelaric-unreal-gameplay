// Copyright (c) 2026 Nelaric

/** @file GameplayComponent.h
 * Declares a gameplay-facing pawn component.
 */

#pragma once

#include "Pawn/NelaricPawnComponent.h"

#include "GameplayComponent.generated.h"

/** @brief Pawn component available to gameplay content.
 *
 * @details Add to a pawn in Blueprint or derive in C++. The pawn owns the
 * component. It inherits the pawn context queries and component defaults.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Nelaric), meta = (BlueprintSpawnableComponent))
class UGameplayComponent : public UNelaricPawnComponent
{
	GENERATED_BODY()

public:
	/// Constructs a gameplay component with the pawn base defaults.
	GAMEPLAYRUNTIME_API UGameplayComponent(const FObjectInitializer& ObjectInitializer);
};
