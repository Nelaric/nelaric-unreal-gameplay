// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSoldierCoverPoint.h Declares a simple authored cover marker. */
#pragma once

#include "GameFramework/Actor.h"
#include "DemoSoldierCoverPoint.generated.h"

/** @brief Marks a navigable standing position behind level collision.
 * @details Place at floor height near cover. The soldier validates navigation
 * and a blocked crouching sightline when selecting it. No reservation or
 * squad policy is implied; world ownership and game-thread access apply.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoSoldierCoverPoint : public AActor
{
	GENERATED_BODY()

public:
	/// Whether this point may be selected by a soldier on authority.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Demo|Soldier")
	bool bEnabled = true;

public:
	ADemoSoldierCoverPoint();
};
