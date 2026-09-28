// Copyright (c) 2026 Nelaric

/** @file NelaricGameplayComponent.h
 * Declares a gameplay-facing pawn component.
 */

#pragma once

#include "Pawn/NelaricPawnComponent.h"

#include "NelaricGameplayComponent.generated.h"

/** @brief Pawn component available to gameplay content.
 *
 * @details Add to a pawn in Blueprint or derive in C++. The pawn owns the
 * component. It inherits the pawn context queries and component defaults.
 */
UCLASS(MinimalAPI, Blueprintable, ClassGroup = (Nelaric), meta = (BlueprintSpawnableComponent))
class UNelaricGameplayComponent : public UNelaricPawnComponent
{
	GENERATED_BODY()

public:
	/// Constructs a gameplay component with the pawn base defaults.
	NELARICFOUNDATION_API UNelaricGameplayComponent(const FObjectInitializer& ObjectInitializer);
};
