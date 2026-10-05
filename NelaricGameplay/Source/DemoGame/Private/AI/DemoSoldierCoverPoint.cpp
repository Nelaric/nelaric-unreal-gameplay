// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSoldierCoverPoint.h"
#include "Components/SceneComponent.h"

ADemoSoldierCoverPoint::ADemoSoldierCoverPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CoverLocation"));
}
