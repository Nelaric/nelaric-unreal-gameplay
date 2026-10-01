// Copyright (c) 2026 Nelaric Contributors

#include "Player/DemoOverviewPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Pawn/PawnControlComponent.h"

ADemoOverviewPawn::ADemoOverviewPawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	bOnlyRelevantToOwner = true;
	SetReplicates(true);
	SetReplicateMovement(false);
	SetCanBeDamaged(false);
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("OverviewRoot")));
	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("OverviewCamera"));
	CameraComponent->SetupAttachment(GetRootComponent());
	CameraComponent->SetRelativeRotation(FRotator(-60.0, 0.0, 0.0));
	CameraComponent->bUsePawnControlRotation = false;
	MovementComponent = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("OverviewMovement"));
	MovementComponent->SetUpdatedComponent(GetRootComponent());

	ControlPolicy = CreateDefaultSubobject<UPawnControlComponent>(TEXT("ControlPolicy"));
	ControlPolicy->bAllowPlayerControl = true;
	ControlPolicy->bAllowReturnControl = true;
	ControlPolicy->bReturnToBot = false;
	ControlPolicy->bStartBotLogicOnReady = false;
	ControlPolicy->ReturnControllerClass = nullptr;
}

UCameraComponent* ADemoOverviewPawn::GetCameraComponent() const
{
	return CameraComponent;
}

UPawnControlComponent* ADemoOverviewPawn::GetControlPolicy() const
{
	return ControlPolicy;
}

UPawnMovementComponent* ADemoOverviewPawn::GetMovementComponent() const
{
	return MovementComponent;
}

void ADemoOverviewPawn::UnPossessed()
{
	const TWeakObjectPtr<AController> ReleasingController = GetController();
	Super::UnPossessed();
	MovementComponent->StopMovementImmediately();
	// APawn clears Owner on release. Preserve owner relevancy and permission
	// to return to this camera while the player operates another character.
	if (HasAuthority() && !IsActorBeingDestroyed() && !GetController() && ReleasingController.IsValid() &&
	    !ReleasingController->IsActorBeingDestroyed())
	{
		SetOwner(ReleasingController.Get());
		ForceNetUpdate();
	}
}
