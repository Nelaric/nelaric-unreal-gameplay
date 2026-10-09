// Copyright (c) 2026 Nelaric Contributors

#include "Player/DemoOverviewPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
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
}

UCameraComponent* ADemoOverviewPawn::GetCameraComponent() const
{
	return CameraComponent;
}

UPawnControlComponent* ADemoOverviewPawn::GetControlPolicy() const
{
	TInlineComponentArray<UPawnControlComponent*> Policies(this);
	return Policies.Num() == 1 ? Policies[0] : nullptr;
}

void ADemoOverviewPawn::PanOverview(const FVector2D& LocalOffset)
{
	const APlayerController* LocalController = Cast<APlayerController>(GetController());
	if (!LocalController || !LocalController->IsLocalPlayerController() || LocalController->GetPawn() != this ||
	    IsActorBeingDestroyed() || !GetWorld() || GetWorld()->bIsTearingDown || !FMath::IsFinite(LocalOffset.X) ||
	    !FMath::IsFinite(LocalOffset.Y))
	{
		return;
	}
	const FRotator YawRotation(0.0, CameraComponent->GetComponentRotation().Yaw, 0.0);
	const FVector Offset = YawRotation.RotateVector(FVector(LocalOffset.Y, LocalOffset.X, 0.0));
	const FVector CurrentLocation = GetActorLocation();
	FVector NewLocation = CurrentLocation + Offset;
	NewLocation.Z = CurrentLocation.Z;
	SetActorLocation(NewLocation);
}

void ADemoOverviewPawn::ClientDeployOverview_Implementation(FVector Location)
{
	if (!Location.ContainsNaN() && !IsActorBeingDestroyed())
		SetActorLocation(Location);
}

void ADemoOverviewPawn::BeginPlay()
{
	Super::BeginPlay();
	// The initialization config creates the policy during BeginPlay. This
	// camera stays with its player while a character is controlled; handing
	// it to a bot would replace its owner and prevent returning to it.
	if (UPawnControlComponent* Policy = GetControlPolicy())
	{
		Policy->bReturnToBot = false;
		Policy->bStartBotLogicOnReady = false;
	}
}

void ADemoOverviewPawn::UnPossessed()
{
	const TWeakObjectPtr<AController> ReleasingController = GetController();
	Super::UnPossessed();
	// APawn clears Owner on release. Preserve owner relevancy and permission
	// to return to this camera while the player operates another character.
	if (HasAuthority() && !IsActorBeingDestroyed() && !GetController() && ReleasingController.IsValid() &&
	    !ReleasingController->IsActorBeingDestroyed())
	{
		SetOwner(ReleasingController.Get());
		ForceNetUpdate();
	}
}
