// Copyright (c) 2026 Nelaric Contributors

#include "Pawn/NelaricPawn.h"

#include "Pawn/PawnInitializationHelper.h"
#include "Pawn/PawnInitializationComponent.h"

ANelaricPawn::ANelaricPawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PawnInitializationComponent = Nelaric::Pawn::FInitializationHelper::Create(this, ObjectInitializer);
}

void ANelaricPawn::PossessedBy(AController* NewController)
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::PossessedBy(NewController);
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricPawn::UnPossessed()
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::UnPossessed();
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricPawn::OnRep_Controller()
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::OnRep_Controller();
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricPawn::OnRep_PlayerState()
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::OnRep_PlayerState();
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricPawn::NotifyControllerChanged()
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::NotifyControllerChanged();
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricPawn::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);
	PawnInitializationComponent->EndPawnContextChange();
}

UPawnInitializationComponent* ANelaricPawn::GetPawnInitializationComponent() const
{
	return PawnInitializationComponent;
}
