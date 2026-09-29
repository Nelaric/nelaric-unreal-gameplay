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
	PawnInitializationComponent->BeginPawnContextChange();
	Super::PossessedBy(NewController);
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricCharacter::UnPossessed()
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::UnPossessed();
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricCharacter::OnRep_Controller()
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::OnRep_Controller();
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricCharacter::OnRep_PlayerState()
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::OnRep_PlayerState();
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricCharacter::NotifyControllerChanged()
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::NotifyControllerChanged();
	PawnInitializationComponent->EndPawnContextChange();
}

void ANelaricCharacter::OnPlayerStateChanged(APlayerState* NewPlayerState, APlayerState* OldPlayerState)
{
	PawnInitializationComponent->BeginPawnContextChange();
	Super::OnPlayerStateChanged(NewPlayerState, OldPlayerState);
	PawnInitializationComponent->EndPawnContextChange();
}

UPawnInitializationComponent* ANelaricCharacter::GetPawnInitializationComponent() const
{
	return PawnInitializationComponent;
}
