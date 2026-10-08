// Copyright (c) 2026 Nelaric Contributors

#include "GAS/DemoGasPlayerState.h"
#include "GAS/DemoCombatAttributes.h"
#include "Net/UnrealNetwork.h"
ADemoGasPlayerState::ADemoGasPlayerState()
{
	AttributeSetClasses.Add(UDemoCombatAttributes::StaticClass());
}

void ADemoGasPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADemoGasPlayerState, BattlefrontTeamId);
}
void ADemoGasPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);
	if (auto* State = Cast<ADemoGasPlayerState>(PlayerState))
		State->BattlefrontTeamId = BattlefrontTeamId;
}
