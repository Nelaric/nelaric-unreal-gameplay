// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSoldierController.h"
#include "AI/DemoSoldierBrainComponent.h"
#include "AI/DemoSoldierComponent.h"
#include "AI/GameAIStateTreeComponent.h"
#include "Character/DemoCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Damage.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISense_Damage.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"

ADemoSoldierController::ADemoSoldierController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bStartAILogicOnPossess = false;
	SoldierBrain = CreateDefaultSubobject<UDemoSoldierBrainComponent>(TEXT("SoldierBrain"));
	BrainComponent = SoldierBrain;
	SoldierPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("SoldierPerception"));
	SetPerceptionComponent(*SoldierPerception);
	Sight = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight"));
	Sight->SightRadius = 6000.0f;
	Sight->LoseSightRadius = 6500.0f;
	Sight->PeripheralVisionAngleDegrees = 75.0f;
	Sight->SetMaxAge(8.0f);
	Sight->DetectionByAffiliation.bDetectEnemies = true;
	Sight->DetectionByAffiliation.bDetectFriendlies = true;
	Sight->DetectionByAffiliation.bDetectNeutrals = true;
	Hearing = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
	Hearing->HearingRange = 4000.0f;
	Hearing->SetMaxAge(4.0f);
	Hearing->DetectionByAffiliation = Sight->DetectionByAffiliation;
	Damage = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("Damage"));
	Damage->SetMaxAge(2.0f);
	SoldierPerception->ConfigureSense(*Sight);
	SoldierPerception->ConfigureSense(*Hearing);
	SoldierPerception->ConfigureSense(*Damage);
	SoldierPerception->SetDominantSense(UAISense_Sight::StaticClass());
	SoldierPerception->OnTargetPerceptionUpdated.AddDynamic(this, &ThisClass::HandlePerception);
}

void ADemoSoldierController::OnPossess(APawn* InPawn)
{
	ClearTeamSubscriptions();
	BrainComponent = bUseNativeBrain ? static_cast<UBrainComponent*>(SoldierBrain)
	                                 : FindComponentByClass<UGameAIStateTreeComponent>();
	if (!BrainComponent && !bUseNativeBrain)
	{
		UE_LOG(LogStateTree, Warning,
		       TEXT("Soldier controller %s requires a Game AI State Tree Component when Use Native Brain is disabled."),
		       *GetName());
	}
	Super::OnPossess(InPawn);
	const ADemoCharacter* DemoPawn = Cast<ADemoCharacter>(InPawn);
	SetGenericTeamId(DemoPawn ? FGenericTeamId(DemoPawn->GetTeamId()) : FGenericTeamId::NoTeam);
	SoldierPerception->ForgetAll();
	SoldierPerception->RequestStimuliListenerUpdate();
}

FVector ADemoSoldierController::GetFocalPointOnActor(const AActor* Actor) const
{
	if (const APawn* FocusedPawn = Cast<APawn>(Actor))
	{
		return FocusedPawn->GetPawnViewLocation();
	}
	return Super::GetFocalPointOnActor(Actor);
}

void ADemoSoldierController::OnUnPossess()
{
	ClearTeamSubscriptions();
	Super::OnUnPossess();
	SoldierPerception->ForgetAll();
}

void ADemoSoldierController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTeamSubscriptions();
	Super::EndPlay(EndPlayReason);
}

void ADemoSoldierController::ClearTeamSubscriptions()
{
	for (const auto& Entry : TeamSubscriptions)
	{
		if (ADemoCharacter* DemoPawn = Entry.Key.Get())
		{
			DemoPawn->OnTeamChanged().Remove(Entry.Value);
		}
	}
	TeamSubscriptions.Reset();
}

void ADemoSoldierController::HandleObservedTeamChanged()
{
	if (UDemoSoldierComponent* Soldier = GetPawn() ? GetPawn()->FindComponentByClass<UDemoSoldierComponent>() : nullptr)
	{
		Soldier->NotifyTeamChanged();
	}
}

ETeamAttitude::Type ADemoSoldierController::GetTeamAttitudeTowards(const AActor& Other) const
{
	const UDemoSoldierComponent* Soldier =
	    GetPawn() ? GetPawn()->FindComponentByClass<UDemoSoldierComponent>() : nullptr;
	if (Soldier && Soldier->IsHostile(const_cast<AActor*>(&Other)))
	{
		return ETeamAttitude::Hostile;
	}
	const ADemoCharacter* OtherCharacter = Cast<ADemoCharacter>(&Other);
	return Soldier && OtherCharacter && Soldier->GetTeamId() != 255 &&
	               Soldier->GetTeamId() == OtherCharacter->GetTeamId()
	           ? ETeamAttitude::Friendly
	           : ETeamAttitude::Neutral;
}

void ADemoSoldierController::HandlePerception(AActor* Actor, FAIStimulus Stimulus)
{
	UDemoSoldierComponent* Soldier = GetPawn() ? GetPawn()->FindComponentByClass<UDemoSoldierComponent>() : nullptr;
	if (!Soldier || !IsValid(Actor) || Actor == GetPawn())
	{
		return;
	}
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		if (ADemoCharacter* DemoPawn = Cast<ADemoCharacter>(Actor))
		{
			if (Stimulus.WasSuccessfullySensed() && !TeamSubscriptions.Contains(DemoPawn))
			{
				TeamSubscriptions.Add(
				    DemoPawn, DemoPawn->OnTeamChanged().AddUObject(this, &ThisClass::HandleObservedTeamChanged));
			}
			else if (!Stimulus.WasSuccessfullySensed())
			{
				FDelegateHandle Handle;
				if (TeamSubscriptions.RemoveAndCopyValue(DemoPawn, Handle))
				{
					DemoPawn->OnTeamChanged().Remove(Handle);
				}
			}
		}
		Soldier->ReportSight(Actor, Stimulus.StimulusLocation, Stimulus.WasSuccessfullySensed());
	}
	else if (Stimulus.WasSuccessfullySensed() && Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>() &&
	         GetTeamAttitudeTowards(*Actor) != ETeamAttitude::Friendly)
	{
		Soldier->ReportSound(Stimulus.StimulusLocation);
	}
	else if (Stimulus.WasSuccessfullySensed() && Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>())
	{
		Soldier->ReportDamage(Actor, Stimulus.Strength, Stimulus.StimulusLocation - GetPawn()->GetActorLocation());
	}
}
