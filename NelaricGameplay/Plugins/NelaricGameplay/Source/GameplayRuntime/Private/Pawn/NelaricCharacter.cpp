// Copyright (c) 2026 Nelaric

#include "Pawn/NelaricCharacter.h"

#include "Pawn/PawnInitializationHelper.h"
#include "Pawn/PawnInitializationComponent.h"

ANelaricCharacter::ANelaricCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PawnInitializationComponent = Nelaric::Pawn::FInitializationHelper::Create(this, ObjectInitializer);
}

void ANelaricCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	PawnInitializationComponent->InvalidatePawnContext();
}

void ANelaricCharacter::UnPossessed()
{
	Super::UnPossessed();
	PawnInitializationComponent->InvalidatePawnContext();
}

void ANelaricCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	PawnInitializationComponent->InvalidatePawnContext();
}

void ANelaricCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	PawnInitializationComponent->InvalidatePawnContext();
}

void ANelaricCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PawnInitializationComponent->InvalidatePawnContext();
}

UPawnInitializationComponent* ANelaricCharacter::GetPawnInitializationComponent() const
{
	return PawnInitializationComponent;
}
