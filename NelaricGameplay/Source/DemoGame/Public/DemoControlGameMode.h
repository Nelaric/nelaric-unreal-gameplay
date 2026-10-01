// Copyright (c) 2026 Nelaric Contributors

/** @file DemoControlGameMode.h
 * Declares overview pawn spawning and owner-only control policy.
 */

#pragma once

#include "DemoGameMode.h"

#include "DemoControlGameMode.generated.h"

/** @brief Starts participants with a persistent overview camera pawn.
 * @details The authority world owns this game
 * mode. It retains the demo's
 * shared GAS PlayerState and selects ADemoPlayerController. Only the owning
 * player
 * may take an overview pawn. Blueprint subclasses configure the
 * default overview pawn class. Operations run on the
 * game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoControlGameMode : public ADemoGameMode
{
	GENERATED_BODY()

public:
	/// World-space offset in centimeters added to the selected player start.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Demo|Camera")
	FVector OverviewSpawnOffset;

public:
	DEMOGAME_API ADemoControlGameMode();
	DEMOGAME_API virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer,
	                                                                       const FTransform& SpawnTransform) override;
	DEMOGAME_API virtual bool CanChangePawnControl_Implementation(AController* Requester, EControlSwitchAction Action,
	                                                              APawn* TargetPawn) const override;
};
