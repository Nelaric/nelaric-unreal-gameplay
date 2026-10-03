// Copyright (c) 2026 Nelaric Contributors

/** @file DemoRuntimeCharacterSpawnPoint.h
 * Declares the placeholder for demo runtime character spawn points.
 */

#pragma once

#include "Engine/TargetPoint.h"

#include "DemoRuntimeCharacterSpawnPoint.generated.h"

/** @brief Marks a location reserved for future runtime character spawning.
 * @details This placeable Blueprint base
 * currently has no spawning behavior.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoRuntimeCharacterSpawnPoint : public ATargetPoint
{
	GENERATED_BODY()

public:
public:
	DEMOGAME_API ADemoRuntimeCharacterSpawnPoint(const FObjectInitializer& ObjectInitializer);
};
