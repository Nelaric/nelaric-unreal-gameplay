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
	CreateConfiguredComponents();
	bInitializationAllowed = true;
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

bool UNelaricPawnInitializationComponent::ValidateConfiguration() const
{
	APawn* Owner = GetPawn();
	TMap<FName, const FNelaricPawnInitializationEntry*> EntriesById;
	for (int32 Index = 0; Index < InitializationConfig->Components.Num(); ++Index)
	{
		const FNelaricPawnInitializationEntry& Entry = InitializationConfig->Components[Index];
		const TCHAR* Reason = nullptr;
		UClass* Class = Entry.ComponentClass.Get();
		if (Entry.ComponentId.IsNone())
		{
			Reason = TEXT("empty component ID");
		}
		else if (EntriesById.Contains(Entry.ComponentId))
		{
			Reason = TEXT("duplicate component ID");
		}
		else if (!Class)
		{
			Reason = TEXT("missing component class");
		}
		else if (Class->HasAnyClassFlags(CLASS_Abstract))
		{
			Reason = TEXT("abstract component class");
		}
		else if (!Class->ImplementsInterface(UNelaricInitStateParticipantInterface::StaticClass()))
		{
			Reason = TEXT("component class does not implement the init-state participant interface");
		}
		if (Reason)
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid pawn initialization entry [%d] '%s' on %s: %s."), Index,
			       *Entry.ComponentId.ToString(), *GetNameSafe(Owner), Reason);
			return false;
		}
		EntriesById.Add(Entry.ComponentId, &Entry);
	}
	for (int32 Index = 0; Index < InitializationConfig->Components.Num(); ++Index)
	{
		const FNelaricPawnInitializationEntry& Entry = InitializationConfig->Components[Index];
		TSet<FName> DeclaredDependencies;
		for (FName DependencyId : Entry.DependencyIds)
		{
			const TCHAR* Reason = nullptr;
			const FNelaricPawnInitializationEntry* const* Dependency = EntriesById.Find(DependencyId);
			if (DependencyId.IsNone())
			{
				Reason = TEXT("empty dependency ID");
			}
			else if (DeclaredDependencies.Contains(DependencyId))
			{
				Reason = TEXT("duplicate dependency declaration");
			}
			else if (!Dependency)
			{
				Reason = TEXT("dependency ID does not exist");
			}
			else if (Entry.bCreateOnAuthority && !(*Dependency)->bCreateOnAuthority)
			{
				Reason = TEXT("dependency is not created on authority");
			}
			else if (Entry.bCreateOnClient && !(*Dependency)->bCreateOnClient)
			{
				Reason = TEXT("dependency is not created on clients");
			}
			if (Reason)
			{
				UE_LOG(LogTemp, Error, TEXT("Invalid pawn initialization entry [%d] '%s' on %s: dependency '%s' %s."),
				       Index, *Entry.ComponentId.ToString(), *GetNameSafe(Owner), *DependencyId.ToString(), Reason);
				return false;
			}
			DeclaredDependencies.Add(DependencyId);
		}
	}
	for (int32 Index = 0; Index < InitializationConfig->Components.Num(); ++Index)
	{
		const FNelaricPawnInitializationEntry& Entry = InitializationConfig->Components[Index];
		if (Owner->HasAuthority() ? !Entry.bCreateOnAuthority : !Entry.bCreateOnClient)
		{
			continue;
		}
		const FName InstanceName(*FString::Printf(TEXT("NelaricInit_%s"), *Entry.ComponentId.ToString()));
		if (UObject* Existing = FindObject<UObject>(Owner, *InstanceName.ToString()))
		{
			if (!Existing->IsA(Entry.ComponentClass))
			{
				UE_LOG(
				    LogTemp, Error,
				    TEXT("Invalid pawn initialization entry [%d] '%s' on %s: instance '%s' has a conflicting class."),
				    Index, *Entry.ComponentId.ToString(), *GetNameSafe(Owner), *InstanceName.ToString());
				return false;
			}
		}
	}
	return true;
}

void UNelaricPawnInitializationComponent::CreateConfiguredComponents()
{
	if (bConfiguredComponentsCreated || !bConfigValid || !InitializationConfig || !GetPawn())
	{
		return;
	}
	APawn* Owner = GetPawn();
	if (!ValidateConfiguration())
	{
		bConfigValid = false;
		return;
	}
	UWorld* World = GetWorld();
	UNelaricInitStateWorldSubsystem* Subsystem =
	    World ? World->GetSubsystem<UNelaricInitStateWorldSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("Pawn initialization on %s has no init-state world subsystem."),
		       *GetNameSafe(Owner));
		bConfigValid = false;
		return;
	}

	TArray<UActorComponent*> ToRegister;
	// Build the entire ID table before resolving any dependencies or registering components.
	for (const FNelaricPawnInitializationEntry& Entry : InitializationConfig->Components)
	{
		if (Owner->HasAuthority() ? !Entry.bCreateOnAuthority : !Entry.bCreateOnClient)
		{
			continue;
		}
		const FName InstanceName(*FString::Printf(TEXT("NelaricInit_%s"), *Entry.ComponentId.ToString()));
		UActorComponent* Component = ConfiguredComponents.FindRef(Entry.ComponentId).Get();
		if (!Component)
		{
			Component = FindObject<UActorComponent>(Owner, *InstanceName.ToString());
		}
		if (!Component)
		{
			Component = NewObject<UActorComponent>(Owner, Entry.ComponentClass, InstanceName);
			Owner->AddInstanceComponent(Component);
		}
		ConfiguredComponents.Add(Entry.ComponentId, Component);
		if (!Component->IsRegistered())
		{
			ToRegister.Add(Component);
		}
	}
	for (const FNelaricPawnInitializationEntry& Entry : InitializationConfig->Components)
	{
		UActorComponent* Component = ConfiguredComponents.FindRef(Entry.ComponentId).Get();
		if (!Component)
		{
			continue;
		}
		TArray<UActorComponent*> Dependencies;
		Dependencies.Reserve(Entry.DependencyIds.Num());
		for (FName DependencyId : Entry.DependencyIds)
		{
			UActorComponent* Dependency = ConfiguredComponents.FindRef(DependencyId).Get();
			check(Dependency);
			Dependencies.Add(Dependency);
		}
		Subsystem->ConfigureParticipant(Component, Entry.ComponentId, Entry.bRequiredForPawnReady, Dependencies);
	}
	bConfiguredComponentsCreated = true;
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
		if (!IsValid(ConfiguredComponents.FindRef(Entry.ComponentId).Get()))
		{
			return false;
		}
	}
	const UWorld* World = GetWorld();
	const UNelaricInitStateWorldSubsystem* Subsystem =
	    World ? World->GetSubsystem<UNelaricInitStateWorldSubsystem>() : nullptr;
	return Subsystem && Subsystem->AreRequiredParticipantsReady(Owner);
}
