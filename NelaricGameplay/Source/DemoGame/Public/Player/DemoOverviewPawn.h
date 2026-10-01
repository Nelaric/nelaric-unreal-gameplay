// Copyright (c) 2026 Nelaric Contributors

/** @file DemoOverviewPawn.h
 * Declares the player's persistent overview camera pawn.
 */

#pragma once

#include "Pawn/NelaricPawn.h"

#include "DemoOverviewPawn.generated.h"

class UCameraComponent;
class UPawnControlComponent;
class UFloatingPawnMovement;

/** @brief Holds a persistent overview camera for one player.
 * @details The authority spawns this pawn for its
 * player.
 * It replicates only to that player and retains Owner when released.
 * The controller destroys it at
 * teardown. No GAS state is installed.
 * It uses the controlling player's PlayerState.
 * Access components on the
 * game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoOverviewPawn : public ANelaricPawn
{
	GENERATED_BODY()

public:
	/// Returns the camera component owned by this pawn.
	UFUNCTION(BlueprintPure, Category = "Demo|Camera")
	DEMOGAME_API UCameraComponent* GetCameraComponent() const;

	/// Returns the owned policy allowing release without bot handback.
	UFUNCTION(BlueprintPure, Category = "Demo|Control")
	DEMOGAME_API UPawnControlComponent* GetControlPolicy() const;

public:
	DEMOGAME_API ADemoOverviewPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	DEMOGAME_API virtual void UnPossessed() override;
	DEMOGAME_API virtual UPawnMovementComponent* GetMovementComponent() const override;

protected:
	/// Camera owned by this pawn; Blueprint subclasses configure its lens.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Demo|Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

	/// Control policy owned by this pawn; release never requires a bot.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Demo|Control")
	TObjectPtr<UPawnControlComponent> ControlPolicy;

	/// Local flying movement owned by this pawn; Blueprint configures speed.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Demo|Camera")
	TObjectPtr<UFloatingPawnMovement> MovementComponent;
};
