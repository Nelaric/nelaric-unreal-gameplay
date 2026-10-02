// Copyright (c) 2026 Nelaric Contributors

#include "NelaricGasCharacter.h"
#include "NelaricAbilitySystemComponent.h"
#include "PawnGasBindingComponent.h"
#include "AI/NelaricBotController.h"

ANelaricGasCharacter::ANelaricGasCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	GasBinding = CreateDefaultSubobject<UPawnGasBindingComponent>(TEXT("GasBinding"));
	AIControllerClass = ANelaricBotController::StaticClass();
}
UAbilitySystemComponent* ANelaricGasCharacter::GetAbilitySystemComponent() const
{
	return GasBinding->GetAbilitySystem();
}
UPawnGasBindingComponent* ANelaricGasCharacter::GetGasBinding() const
{
	return GasBinding;
}
