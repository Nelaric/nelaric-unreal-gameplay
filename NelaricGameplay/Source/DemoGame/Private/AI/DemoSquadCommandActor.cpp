// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSquadCommandActor.h"
#include "AI/DemoSquadContextComponent.h"
#include "AI/DemoSquadMemberComponent.h"
#include "AI/GameAIStateTreeComponent.h"
#include "Character/DemoCharacter.h"

ADemoSquadCommandActor::ADemoSquadCommandActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
	SquadContext = CreateDefaultSubobject<UDemoSquadContextComponent>(TEXT("SquadContext"));
	GameAITree = CreateDefaultSubobject<UGameAIStateTreeComponent>(TEXT("GameAITree"));
	GameAITree->SetStartLogicAutomatically(false);
}

UDemoSquadContextComponent* ADemoSquadCommandActor::GetSquadContext() const
{
	return SquadContext;
}

bool ADemoSquadCommandActor::StartCommander()
{
	check(IsInGameThread());
	if (!HasAuthority())
	{
		return false;
	}
	for (ADemoCharacter* Character : InitialMembers)
	{
		if (IsValid(Character))
		{
			if (auto* Member = Character->FindComponentByClass<UDemoSquadMemberComponent>())
			{
				Member->JoinSquad(this, {});
			}
		}
	}
	if (!IsValid(SquadContext) || !IsValid(GameAITree) || SquadContext->GetMembers().IsEmpty())
	{
		return false;
	}
	GameAITree->StartLogic();
	return GameAITree->IsRunning();
}

void ADemoSquadCommandActor::StopCommander()
{
	check(IsInGameThread());
	if (!HasAuthority())
	{
		return;
	}
	if (IsValid(GameAITree))
	{
		GameAITree->StopLogic(TEXT("Squad commander stopped"));
	}
	if (IsValid(SquadContext))
	{
		SquadContext->StopExecution(GameAITree);
	}
}

void ADemoSquadCommandActor::BeginPlay()
{
	Super::BeginPlay();
	if (bStartOnBeginPlay)
	{
		StartCommander();
	}
}

void ADemoSquadCommandActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopCommander();
	Super::EndPlay(EndPlayReason);
}
