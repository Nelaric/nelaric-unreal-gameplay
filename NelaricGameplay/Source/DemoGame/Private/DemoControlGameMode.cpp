// Copyright (c) 2026 Nelaric Contributors

#include "DemoControlGameMode.h"

#include "Player/DemoPlayerController.h"
#include "Player/DemoOverviewPawn.h"

ADemoControlGameMode::ADemoControlGameMode()
{
	PlayerControllerClass = ADemoPlayerController::StaticClass();
	DefaultPawnClass = ADemoOverviewPawn::StaticClass();
	bStartPlayersAsSpectators = false;
	OverviewSpawnOffset = FVector(0.0, 0.0, 1800.0);
}

APawn* ADemoControlGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer,
                                                                        const FTransform& SpawnTransform)
{
	UClass* OverviewClass = GetDefaultPawnClassForController(NewPlayer);
	if (!OverviewClass || !OverviewClass->IsChildOf(ADemoOverviewPawn::StaticClass()))
	{
		return nullptr;
	}
	FTransform OverviewTransform = SpawnTransform;
	OverviewTransform.AddToTranslation(OverviewSpawnOffset);
	APawn* SpawnedPawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, OverviewTransform);
	if (IsValid(SpawnedPawn))
	{
		SpawnedPawn->SetOwner(NewPlayer);
	}
	return SpawnedPawn;
}

bool ADemoControlGameMode::CanChangePawnControl_Implementation(AController* Requester, EControlSwitchAction Action,
                                                               APawn* TargetPawn) const
{
	if (!Super::CanChangePawnControl_Implementation(Requester, Action, TargetPawn))
	{
		return false;
	}
	if (const ADemoOverviewPawn* Overview = Cast<ADemoOverviewPawn>(TargetPawn))
	{
		return IsValid(Requester) && Overview->GetOwner() == Requester && Requester->IsPlayerController();
	}
	return true;
}
