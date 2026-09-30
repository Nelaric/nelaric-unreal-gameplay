// Copyright (c) 2026 Nelaric Contributors

/** @file NelaricPawn.h
 * Declares the project's general-purpose pawn base.
 */

#pragma once

#include "GameFramework/Pawn.h"

#include "NelaricPawn.generated.h"

class UPawnInitializationComponent;

namespace Nelaric::Pawn
{
struct FInitializationHelper;
}

/** @brief Base pawn for project-specific controllable actors.
 *
 * @details Derive in C++ or Blueprint and compose gameplay with pawn
 * components. Retains APawn defaults, including its movement behavior.
 * The world manages actor lifetime. Access on the game thread.
 */
UCLASS(MinimalAPI, Blueprintable)
class ANelaricPawn : public APawn
{
	GENERATED_BODY()

public:
	/** @brief Constructs the pawn with the engine's default behavior.
	 *
	 * @details Called by Unreal on the game thread during actor creation.
	 * @param ObjectInitializer Initializer supporting derived subobjects.
	 */
	GAMEPLAYRUNTIME_API ANelaricPawn(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** @brief Returns this pawn's local initialization coordinator.
	 * @details The pawn owns the component. Use on the game thread to bind
	 * Ready and revoked notifications or configure initialization.
	 */
	UFUNCTION(BlueprintPure, Category = "Nelaric|Pawn|Initialization")
	GAMEPLAYRUNTIME_API UPawnInitializationComponent* GetPawnInitializationComponent() const;

public:
	GAMEPLAYRUNTIME_API virtual void PossessedBy(AController* NewController) override;
	GAMEPLAYRUNTIME_API virtual void UnPossessed() override;
	GAMEPLAYRUNTIME_API virtual void OnRep_Controller() override;
	GAMEPLAYRUNTIME_API virtual void NotifyControllerChanged() override;
	GAMEPLAYRUNTIME_API virtual void OnRep_PlayerState() override;
	GAMEPLAYRUNTIME_API virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	/** @brief Restarts bindings when Unreal changes the player state.
	 * @details Runs on the game thread, including direct SetPlayerState calls.
	 * Nested possession and replication notifications share one reset.
	 * @param NewPlayerState Newly assigned player state, possibly null.
	 * @param OldPlayerState Previously assigned player state, possibly null.
	 */
	GAMEPLAYRUNTIME_API virtual void OnPlayerStateChanged(APlayerState* NewPlayerState,
	                                                      APlayerState* OldPlayerState) override;

	/// Initialization component owned by this pawn.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Nelaric|Pawn|Initialization")
	TObjectPtr<UPawnInitializationComponent> PawnInitializationComponent;

private:
	friend struct Nelaric::Pawn::FInitializationHelper;
};
