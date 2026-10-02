// Copyright (c) 2026 Nelaric Contributors

#include "DemoControlGameMode.h"

#include "Player/DemoPlayerController.h"
#include "Player/DemoOverviewPawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogDemoControlGameMode, Log, All);

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
		UE_LOG(LogDemoControlGameMode, Error,
		       TEXT("Cannot spawn overview pawn: controller=%s class=%s; expected a DemoOverviewPawn class."),
		       *GetNameSafe(NewPlayer), *GetNameSafe(OverviewClass));
		return nullptr;
	}
	FTransform OverviewTransform = SpawnTransform;
	OverviewTransform.AddToTranslation(OverviewSpawnOffset);
	APawn* SpawnedPawn = Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, OverviewTransform);
	if (IsValid(SpawnedPawn))
	{
		SpawnedPawn->SetOwner(NewPlayer);
	}
	else
	{
		UE_LOG(LogDemoControlGameMode, Error, TEXT("Overview pawn spawn failed: controller=%s class=%s."),
		       *GetNameSafe(NewPlayer), *GetNameSafe(OverviewClass));
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
