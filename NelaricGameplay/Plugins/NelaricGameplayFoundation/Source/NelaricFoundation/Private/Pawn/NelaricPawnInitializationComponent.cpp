// Copyright (c) 2026 Nelaric

#include "Pawn/NelaricPawnInitializationComponent.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Pawn/NelaricPawnInitializationConfig.h"
#include "World/NelaricInitStateParticipantInterface.h"
#include "World/NelaricInitStateWorldSubsystem.h"

UNelaricPawnInitializationComponent::UNelaricPawnInitializationComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

bool UNelaricPawnInitializationComponent::TryInitializePawn()
{
	if (!AreRequiredComponentsReady())
	{
		bPawnInitialized = false;
		return false;
	}
	if (bPawnInitialized)
	{
		return true;
	}

	if (!bInitializationAllowed || !bConfigValid || bInitializationInProgress || !GetPawn())
	{
		return false;
	}

	bInitializationInProgress = true;
	const bool bReady = CanInitializePawn();
	bInitializationInProgress = false;
	if (!bReady || !bInitializationAllowed || !GetPawn())
	{
		return false;
	}

	bPawnInitialized = true;
	OnPawnInitialized.Broadcast(this);
	return true;
}

bool UNelaricPawnInitializationComponent::IsPawnInitialized() const
{
	return bPawnInitialized && AreRequiredComponentsReady();
}

bool UNelaricPawnInitializationComponent::CanInitializePawn_Implementation() const
{
	return GetPawn() != nullptr;
}

void UNelaricPawnInitializationComponent::BeginPlay()
{
	Super::BeginPlay();
	bInitializationAllowed = true;
	CreateConfiguredComponents();
	TryInitializePawn();
}

void UNelaricPawnInitializationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bInitializationAllowed = false;
	bPawnInitialized = false;
	if (UWorld* World = GetWorld())
	{
		if (UNelaricInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UNelaricInitStateWorldSubsystem>())
		{
			for (const auto& Pair : ConfiguredComponents)
			{
				Subsystem->UnconfigureParticipant(Pair.Value.Get());
			}
		}
	}
	ConfiguredComponents.Empty();
	Super::EndPlay(EndPlayReason);
}

void UNelaricPawnInitializationComponent::CreateConfiguredComponents()
{
	if (!InitializationConfig || !GetPawn())
	{
		return;
	}
	APawn* Owner = GetPawn();
	UWorld* World = GetWorld();
	UNelaricInitStateWorldSubsystem* Subsystem =
	    World ? World->GetSubsystem<UNelaricInitStateWorldSubsystem>() : nullptr;
	if (!Subsystem)
	{
		bConfigValid = false;
		return;
	}

	TSet<FName> AllIds;
	for (const FNelaricPawnInitializationEntry& Entry : InitializationConfig->Components)
	{
		UClass* Class = Entry.ComponentClass.Get();
		if (Entry.ComponentId.IsNone() || AllIds.Contains(Entry.ComponentId) || !Class ||
		    Class->HasAnyClassFlags(CLASS_Abstract) ||
		    !Class->ImplementsInterface(UNelaricInitStateParticipantInterface::StaticClass()))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid pawn initialization entry '%s' on %s."), *Entry.ComponentId.ToString(),
			       *GetNameSafe(Owner));
			bConfigValid = false;
			return;
		}
		AllIds.Add(Entry.ComponentId);
	}
	for (const FNelaricPawnInitializationEntry& Entry : InitializationConfig->Components)
	{
		for (FName DependencyId : Entry.DependencyIds)
		{
			const FNelaricPawnInitializationEntry* Dependency = InitializationConfig->Components.FindByPredicate(
			    [DependencyId](const FNelaricPawnInitializationEntry& Candidate)
			    { return Candidate.ComponentId == DependencyId; });
			if (DependencyId.IsNone() || !Dependency || (Entry.bCreateOnAuthority && !Dependency->bCreateOnAuthority) ||
			    (Entry.bCreateOnClient && !Dependency->bCreateOnClient))
			{
				UE_LOG(LogTemp, Error, TEXT("Unavailable pawn initialization dependency '%s' on %s."),
				       *DependencyId.ToString(), *GetNameSafe(Owner));
				bConfigValid = false;
				return;
			}
		}
	}

	TArray<UActorComponent*> ToRegister;
	for (const FNelaricPawnInitializationEntry& Entry : InitializationConfig->Components)
	{
		if (Owner->HasAuthority() ? !Entry.bCreateOnAuthority : !Entry.bCreateOnClient)
		{
			continue;
		}
		const FName InstanceName(*FString::Printf(TEXT("NelaricInit_%s"), *Entry.ComponentId.ToString()));
		UActorComponent* Component = FindObject<UActorComponent>(Owner, *InstanceName.ToString());
		if (Component && !Component->IsA(Entry.ComponentClass))
		{
			UE_LOG(LogTemp, Error, TEXT("Conflicting pawn initialization component '%s' on %s."),
			       *InstanceName.ToString(), *GetNameSafe(Owner));
			bConfigValid = false;
			return;
		}
		if (!Component)
		{
			Component = NewObject<UActorComponent>(Owner, Entry.ComponentClass, InstanceName);
			Owner->AddInstanceComponent(Component);
		}
		ConfiguredComponents.Add(Entry.ComponentId, Component);
		Subsystem->ConfigureParticipant(Component, Entry.ComponentId, Entry.DependencyIds);
		if (!Component->IsRegistered())
		{
			ToRegister.Add(Component);
		}
	}
	for (UActorComponent* Component : ToRegister)
	{
		Component->RegisterComponent();
	}
}

bool UNelaricPawnInitializationComponent::AreRequiredComponentsReady() const
{
	if (!bConfigValid || !InitializationConfig)
	{
		return bConfigValid;
	}
	const APawn* Owner = GetPawn();
	if (!Owner)
	{
		return false;
	}
	for (const FNelaricPawnInitializationEntry& Entry : InitializationConfig->Components)
	{
		if (!Entry.bRequiredForPawnReady ||
		    (Owner->HasAuthority() ? !Entry.bCreateOnAuthority : !Entry.bCreateOnClient))
		{
			continue;
		}
		UActorComponent* Component = ConfiguredComponents.FindRef(Entry.ComponentId).Get();
		const INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Component);
		if (!Participant || !Participant->IsInitApplicable() || Participant->HasTerminalInitFailure() ||
		    Participant->GetInitState() != Nelaric::EInitState::Ready)
		{
			return false;
		}
	}
	return true;
}
