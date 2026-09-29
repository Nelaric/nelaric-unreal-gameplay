// Copyright (c) 2026 Nelaric

#include "Pawn/NelaricPawn.h"

#include "Pawn/PawnInitializationHelper.h"
#include "Pawn/PawnInitializationComponent.h"

ANelaricPawn::ANelaricPawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PawnInitializationComponent = Nelaric::Pawn::FInitializationHelper::Create(this, ObjectInitializer);
}

void ANelaricPawn::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	PawnInitializationComponent->InvalidatePawnContext();
}

void ANelaricPawn::UnPossessed()
{
	Super::UnPossessed();
	PawnInitializationComponent->InvalidatePawnContext();
}

void ANelaricPawn::OnRep_Controller()
{
	Super::OnRep_Controller();
	PawnInitializationComponent->InvalidatePawnContext();
}

void ANelaricPawn::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	PawnInitializationComponent->InvalidatePawnContext();
}

void ANelaricPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PawnInitializationComponent->InvalidatePawnContext();
}

UPawnInitializationComponent* ANelaricPawn::GetPawnInitializationComponent() const
{
	return PawnInitializationComponent;
}
