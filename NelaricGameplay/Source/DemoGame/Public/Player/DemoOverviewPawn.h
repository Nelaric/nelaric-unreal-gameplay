// Copyright (c) 2026 Nelaric Contributors

/** @file DemoOverviewPawn.h
 * Declares the player's persistent overview camera pawn.
 */

#pragma once

#include "Pawn/NelaricPawn.h"

#include "DemoOverviewPawn.generated.h"

class UCameraComponent;
class UPawnControlComponent;

/** @brief Holds a persistent overview camera for one player.
 * @details The authority spawns this pawn for its
 * player.
 * It replicates only to that player and retains Owner when released.
 * The controller destroys it at
 * teardown. No GAS state is installed.
 * It uses the controlling player's PlayerState.
 * Access components on the
 * game thread.
 * Configure one control policy through pawn initialization or Blueprint.
 */
UCLASS(MinimalAPI, Blueprintable)
class ADemoOverviewPawn : public ANelaricPawn
{
	GENERATED_BODY()

public:
	/// Returns the camera component owned by this pawn.
	UFUNCTION(BlueprintPure, Category = "Demo|Camera")
	DEMOGAME_API UCameraComponent* GetCameraComponent() const;

	/** @brief Finds this pawn's explicitly configured control policy.
	 * @details Borrowed on the game thread during
	 * the component lifetime.
	 * @return The only policy, or null if missing or ambiguous.
	 */
	UFUNCTION(BlueprintPure, Category = "Demo|Control")
	DEMOGAME_API UPawnControlComponent* GetControlPolicy() const;

	/** @brief Moves the local camera without changing its height or rotation.
	 * @details Game-thread only. Invalid offsets are ignored.
	 * @param LocalOffset Centimeters right (X) and forward (Y) by camera yaw.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Camera")
	DEMOGAME_API void PanOverview(const FVector2D& LocalOffset);

	/// Moves the owner camera; authority sends at region changes on game thread.
	UFUNCTION(Client, Reliable)
	void ClientDeployOverview(FVector Location);

public:
	DEMOGAME_API ADemoOverviewPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	DEMOGAME_API virtual void BeginPlay() override;
	DEMOGAME_API virtual void UnPossessed() override;

protected:
	/// Camera owned by this pawn; Blueprint subclasses configure its lens.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Demo|Camera")
	TObjectPtr<UCameraComponent> CameraComponent;
};
