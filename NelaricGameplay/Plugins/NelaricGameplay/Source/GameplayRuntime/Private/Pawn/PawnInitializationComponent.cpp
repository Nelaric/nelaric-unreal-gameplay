// Copyright (c) 2026 Nelaric

#include "Pawn/PawnInitializationComponent.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Pawn/PawnInitializationConfig.h"
#include "Pawn/InitStateParticipantInterface.h"
#include "Pawn/InitStateWorldSubsystem.h"

UPawnInitializationComponent::UPawnInitializationComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

bool UPawnInitializationComponent::TryInitializePawn()
{
	if (bInitializationInProgress)
	{
		return false;
	}
	if (bInitializationAllowed && ActiveConfig != InitializationConfig)
	{
		bInitializationInProgress = true;
		RevokePawnReady();
		DestroyConfiguredComponents();
		ActiveConfig = InitializationConfig;
		bConfigValid = true;
		CreateConfiguredComponents();
		bInitializationInProgress = false;
	}
	if (!AreRequiredComponentsReady())
	{
		RevokePawnReady();
		return false;
	}

	if (!bInitializationAllowed || !bConfigValid || bInitializationInProgress || !GetPawn())
	{
		return false;
	}

	bInitializationInProgress = true;
	const bool bReady = CanInitializePawn();
	bInitializationInProgress = false;
	if (bInitializationAllowed && ActiveConfig != InitializationConfig)
	{
		return TryInitializePawn();
	}
	if (!bReady || !bInitializationAllowed || !GetPawn() || !AreRequiredComponentsReady() ||
	    ActiveConfig != InitializationConfig)
	{
		RevokePawnReady();
		return false;
	}
	if (bPawnInitialized)
	{
		return true;
	}

	bPawnInitialized = true;
	OnPawnInitialized.Broadcast(this);
	return IsPawnInitialized();
}

bool UPawnInitializationComponent::IsPawnInitialized() const
{
	return bPawnInitialized && bInitializationAllowed && ActiveConfig == InitializationConfig &&
	       AreRequiredComponentsReady() && CanInitializePawn();
}

void UPawnInitializationComponent::SetInitializationConfig(UPawnInitializationConfig* NewConfig)
{
	if (InitializationConfig == NewConfig)
	{
		return;
	}
	InitializationConfig = NewConfig;
	if (bInitializationAllowed)
	{
		TryInitializePawn();
	}
}

bool UPawnInitializationComponent::CanInitializePawn_Implementation() const
{
	return GetPawn() != nullptr;
}

bool UPawnInitializationComponent::IsConfiguredInstance(FName ComponentId, const UActorComponent* Component) const
{
	return ConfiguredComponents.FindRef(ComponentId).Get() == Component && Component != nullptr;
}

bool UPawnInitializationComponent::HasConfiguredId(FName ComponentId) const
{
	return ConfiguredComponents.Contains(ComponentId);
}

void UPawnInitializationComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bInitializationEnded)
	{
		return;
	}
	WorldBeginTearDownHandle =
	    FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UPawnInitializationComponent::HandleWorldBeginTearDown);
	ActiveConfig = InitializationConfig;
	CreateConfiguredComponents();
	bInitializationAllowed = true;
	TryInitializePawn();
}

void UPawnInitializationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FWorldDelegates::OnWorldBeginTearDown.Remove(WorldBeginTearDownHandle);
	ShutdownInitialization();
	Super::EndPlay(EndPlayReason);
}

void UPawnInitializationComponent::HandleWorldBeginTearDown(UWorld* World)
{
	if (World == GetWorld())
	{
		ShutdownInitialization();
	}
}

void UPawnInitializationComponent::ShutdownInitialization()
{
	if (bInitializationEnded)
	{
		return;
	}
	bInitializationEnded = true;
	bInitializationAllowed = false;
	RevokePawnReady();
	DestroyConfiguredComponents();
	ActiveConfig = nullptr;
}

void UPawnInitializationComponent::RevokePawnReady()
{
	if (bPawnInitialized)
	{
		bPawnInitialized = false;
		OnPawnInitializationRevoked.Broadcast(this);
	}
}

void UPawnInitializationComponent::DestroyConfiguredComponents()
{
	// Clear the round before teardown callbacks can re-enter the manager.
	TMap<FName, TObjectPtr<UActorComponent>> OldComponents = MoveTemp(ConfiguredComponents);
	ConfiguredComponents.Empty();
	RequiredComponentIds.Empty();
	bConfiguredComponentsCreated = false;
	TArray<UActorComponent*> ToStop;
	ToStop.Reserve(OldComponents.Num());
	for (const auto& Pair : OldComponents)
	{
		if (UActorComponent* Component = Pair.Value.Get())
		{
			ToStop.Add(Component);
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (UInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UInitStateWorldSubsystem>())
		{
			Subsystem->StopConfiguredParticipants(ToStop);
		}
	}
	for (UActorComponent* Component : ToStop)
	{
		if (!IsValid(Component))
		{
			continue;
		}
		if (IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Component))
		{
			Participant->InvalidateInitGeneration();
		}
		// Release the stable name so the next round can create a new instance.
		Component->Rename(nullptr, nullptr, REN_DontCreateRedirectors | REN_NonTransactional);
		Component->DestroyComponent();
	}
}

bool UPawnInitializationComponent::ValidateConfiguration() const
{
	APawn* Owner = GetPawn();
	TMap<FName, const FPawnInitializationEntry*> EntriesById;
	for (int32 Index = 0; Index < ActiveConfig->Components.Num(); ++Index)
	{
		const FPawnInitializationEntry& Entry = ActiveConfig->Components[Index];
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
		else if (!Class->ImplementsInterface(UInitStateParticipantInterface::StaticClass()))
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
	for (int32 Index = 0; Index < ActiveConfig->Components.Num(); ++Index)
	{
		const FPawnInitializationEntry& Entry = ActiveConfig->Components[Index];
		TSet<FName> DeclaredDependencies;
		for (FName DependencyId : Entry.DependencyIds)
		{
			const TCHAR* Reason = nullptr;
			const FPawnInitializationEntry* const* Dependency = EntriesById.Find(DependencyId);
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
	for (int32 Index = 0; Index < ActiveConfig->Components.Num(); ++Index)
	{
		const FPawnInitializationEntry& Entry = ActiveConfig->Components[Index];
		if (Owner->HasAuthority() ? !Entry.bCreateOnAuthority : !Entry.bCreateOnClient)
		{
			continue;
		}
		const FName InstanceName(*FString::Printf(TEXT("Init_%s"), *Entry.ComponentId.ToString()));
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
			UE_LOG(LogTemp, Error,
			       TEXT("Invalid pawn initialization entry [%d] '%s' on %s: instance '%s' already exists outside the "
			            "initialization component."),
			       Index, *Entry.ComponentId.ToString(), *GetNameSafe(Owner), *InstanceName.ToString());
			return false;
		}
		if (const UWorld* World = GetWorld())
		{
			if (const UInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UInitStateWorldSubsystem>())
			{
				if (Subsystem->HasConfiguredId(Owner, Entry.ComponentId))
				{
					UE_LOG(
					    LogTemp, Error,
					    TEXT("Invalid pawn initialization entry [%d] '%s' on %s: component ID is already configured."),
					    Index, *Entry.ComponentId.ToString(), *GetNameSafe(Owner));
					return false;
				}
			}
		}
	}
	return true;
}

void UPawnInitializationComponent::CreateConfiguredComponents()
{
	if (bConfiguredComponentsCreated || !bConfigValid || !GetPawn())
	{
		return;
	}
	if (!ActiveConfig)
	{
		bConfiguredComponentsCreated = true;
		return;
	}
	APawn* Owner = GetPawn();
	if (!ValidateConfiguration())
	{
		bConfigValid = false;
		return;
	}
	UWorld* World = GetWorld();
	UInitStateWorldSubsystem* Subsystem = World ? World->GetSubsystem<UInitStateWorldSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("Pawn initialization on %s has no init-state world subsystem."),
		       *GetNameSafe(Owner));
		bConfigValid = false;
		return;
	}

	TArray<UActorComponent*> ToRegister;
	// Build the entire ID table before resolving any dependencies or registering components.
	for (const FPawnInitializationEntry& Entry : ActiveConfig->Components)
	{
		if (Owner->HasAuthority() ? !Entry.bCreateOnAuthority : !Entry.bCreateOnClient)
		{
			continue;
		}
		const FName InstanceName(*FString::Printf(TEXT("Init_%s"), *Entry.ComponentId.ToString()));
		UActorComponent* Component = NewObject<UActorComponent>(Owner, Entry.ComponentClass, InstanceName);
		// The matching client entry creates its own instance; never replicate this dynamic instance.
		Component->SetIsReplicated(false);
		Owner->AddInstanceComponent(Component);
		ConfiguredComponents.Add(Entry.ComponentId, Component);
		if (Entry.bRequiredForPawnReady)
		{
			RequiredComponentIds.Add(Entry.ComponentId);
		}
		if (!Component->IsRegistered())
		{
			ToRegister.Add(Component);
		}
	}
	TArray<Nelaric::FInitParticipantConfiguration> LocalGraph;
	for (const FPawnInitializationEntry& Entry : ActiveConfig->Components)
	{
		UActorComponent* Component = ConfiguredComponents.FindRef(Entry.ComponentId).Get();
		if (!Component)
		{
			continue;
		}
		Nelaric::FInitParticipantConfiguration Configuration;
		Configuration.Component = Component;
		Configuration.ComponentId = Entry.ComponentId;
		Configuration.bRequiredForPawnReady = Entry.bRequiredForPawnReady;
		Configuration.Dependencies.Reserve(Entry.DependencyIds.Num());
		for (FName DependencyId : Entry.DependencyIds)
		{
			UActorComponent* Dependency = ConfiguredComponents.FindRef(DependencyId).Get();
			check(Dependency);
			Configuration.Dependencies.Add(Dependency);
		}
		LocalGraph.Add(MoveTemp(Configuration));
	}
	if (!Subsystem->ConfigureParticipants(LocalGraph))
	{
		bConfigValid = false;
		DestroyConfiguredComponents();
		return;
	}
	bConfiguredComponentsCreated = true;
	for (UActorComponent* Component : ToRegister)
	{
		Component->RegisterComponent();
	}
}

bool UPawnInitializationComponent::AreRequiredComponentsReady() const
{
	if (!bConfigValid || !bConfiguredComponentsCreated)
	{
		return false;
	}
	const APawn* Owner = GetPawn();
	if (!Owner)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	const UInitStateWorldSubsystem* Subsystem = World ? World->GetSubsystem<UInitStateWorldSubsystem>() : nullptr;
	if (ActiveConfig && !Subsystem)
	{
		return false;
	}
	for (FName ComponentId : RequiredComponentIds)
	{
		UActorComponent* Component = ConfiguredComponents.FindRef(ComponentId).Get();
		if (!IsValid(Component) || !Subsystem || !Subsystem->IsParticipantReady(Component))
		{
			return false;
		}
	}
	return true;
}
