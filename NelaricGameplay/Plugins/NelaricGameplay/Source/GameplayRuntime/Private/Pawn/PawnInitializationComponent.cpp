// Copyright (c) 2026 Nelaric Contributors

#include "Pawn/PawnInitializationComponent.h"

#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Pawn/PawnInitializationConfig.h"
#include "Pawn/InitStateParticipantInterface.h"
#include "Pawn/InitStateWorldSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricPawnInitialization, Log, All);

namespace Nelaric::Pawn
{
static bool ShouldCreateComponent(const APawn* Owner, const FPawnInitializationEntry& Entry)
{
	if (Entry.bReplicateComponent)
	{
		return Owner->HasAuthority();
	}
	const bool bCreateOnAuthority = Owner->HasAuthority() && Entry.bCreateOnAuthority;
	// Listen servers and standalone worlds also run client-side components.
	const bool bCreateOnClient = Owner->GetNetMode() != NM_DedicatedServer && Entry.bCreateOnClient;
	return bCreateOnAuthority || bCreateOnClient;
}

static bool ShouldIncludeComponent(const APawn* Owner, const FPawnInitializationEntry& Entry)
{
	return Entry.bReplicateComponent || ShouldCreateComponent(Owner, Entry);
}
} // namespace Nelaric::Pawn

UPawnInitializationComponent::UPawnInitializationComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

bool UPawnInitializationComponent::TryInitializePawn()
{
	if (bInitializationEnded || !bInitializationAllowed)
	{
		return false;
	}
	// Defer callback retries until the current initialization pass finishes.
	bRefreshPending = true;
	if (bInitializationInProgress || ContextChangeDepth > 0 || bNotifyingRevocation)
	{
		return false;
	}
	bInitializationInProgress = true;
	int32 Passes = 0;
	while (bRefreshPending && !bInitializationEnded && Passes++ < 8)
	{
		bRefreshPending = false;
		TryInitializationPass();
	}
	bInitializationInProgress = false;
	ensureMsgf(!bRefreshPending || bInitializationEnded, TEXT("Pawn initialization requested too many retries."));
	return IsPawnInitialized();
}

bool UPawnInitializationComponent::TryInitializationPass()
{
	// Clear old bindings before creating components for a replacement config.
	if (bContextResetRequested && !bInitializationEnded)
	{
		bContextResetRequested = false;
		RevokePawnReady();
		if (UWorld* World = GetWorld())
		{
			if (UInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UInitStateWorldSubsystem>())
			{
				Subsystem->InvalidateActorContext(GetOwner());
			}
		}
	}
	if (ActiveConfig != InitializationConfig || (!bConfigurationValidated && !bConfiguredComponentsCreated))
	{
		RevokePawnReady();
		DestroyConfiguredComponents();
		if (bInitializationEnded || !GetPawn())
		{
			return false;
		}
		ActiveConfig = InitializationConfig;
		bConfigValid = true;
		CreateConfiguredComponents();
	}
	else if (!bConfiguredComponentsCreated)
	{
		CreateConfiguredComponents();
	}
	if (bInitializationEnded || !bInitializationAllowed || !GetPawn() || !AreRequiredComponentsReady())
	{
		RevokePawnReady();
		return false;
	}
	const bool bReady = CanInitializePawn();
	if (ActiveConfig != InitializationConfig)
	{
		bRefreshPending = true;
	}
	if (!bReady || bInitializationEnded || bContextResetRequested || !GetPawn() || !AreRequiredComponentsReady() ||
	    ActiveConfig != InitializationConfig)
	{
		RevokePawnReady();
		return false;
	}
	if (!bPawnInitialized)
	{
		bPawnInitialized = true;
		OnPawnInitialized.Broadcast(this);
	}
	return IsPawnInitialized();
}

void UPawnInitializationComponent::InvalidatePawnContext()
{
	if (bInitializationEnded)
	{
		return;
	}
	BeginPawnContextChange();
	EndPawnContextChange();
}

void UPawnInitializationComponent::BeginPawnContextChange()
{
	// Revoke before engine callbacks; nested context changes share one reset.
	++ContextChangeDepth;
	bContextResetRequested = true;
	RevokePawnReady();
}

void UPawnInitializationComponent::EndPawnContextChange()
{
	check(ContextChangeDepth > 0);
	--ContextChangeDepth;
	if (ContextChangeDepth == 0)
	{
		TryInitializePawn();
	}
}

void UPawnInitializationComponent::RegisterAndCallPawnInitialized(FPawnInitializationCallback Callback)
{
	if (!Callback.IsBound())
	{
		return;
	}
	// Late subscribers also observe an already initialized pawn.
	OnPawnInitialized.AddUnique(Callback);
	if (IsPawnInitialized())
	{
		Callback.ExecuteIfBound(this);
	}
}

void UPawnInitializationComponent::RegisterPawnInitializationRevoked(FPawnInitializationCallback Callback)
{
	if (Callback.IsBound())
	{
		OnPawnInitializationRevoked.AddUnique(Callback);
	}
}

void UPawnInitializationComponent::UnregisterPawnInitializationCallback(FPawnInitializationCallback Callback)
{
	OnPawnInitialized.Remove(Callback);
	OnPawnInitializationRevoked.Remove(Callback);
}

bool UPawnInitializationComponent::IsPawnInitialized() const
{
	return bPawnInitialized && bInitializationAllowed && !bInitializationEnded && !bContextResetRequested &&
	       ContextChangeDepth == 0 && !bNotifyingRevocation && ActiveConfig == InitializationConfig &&
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
	bInitializationAllowed = true;
	SetComponentTickEnabled(!GetPawn() || !GetPawn()->HasAuthority());
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
		// All revocation listeners finish before a callback can restart gameplay.
		bNotifyingRevocation = true;
		OnPawnInitializationRevoked.Broadcast(this);
		bNotifyingRevocation = false;
	}
}

void UPawnInitializationComponent::DestroyConfiguredComponents()
{
	// Clear the round before teardown callbacks can re-enter the manager.
	TMap<FName, TObjectPtr<UActorComponent>> OldComponents = MoveTemp(ConfiguredComponents);
	ConfiguredComponents.Empty();
	RequiredComponentIds.Empty();
	bConfiguredComponentsCreated = false;
	bConfigurationValidated = false;
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
		if (!IsValid(Component))
		{
			continue;
		}
		// Clients only detach replicated instances; the actor channel owns them.
		if (Component->GetIsReplicated() && GetOwner() && !GetOwner()->HasAuthority())
		{
			continue;
		}
		if (Component->GetIsReplicated() && GetOwner() && GetOwner()->HasAuthority())
		{
			GetOwner()->DestroyReplicatedSubObjectOnRemotePeers(Component);
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
		else if (Entry.bReplicateComponent && (!Entry.bCreateOnAuthority || !Entry.bCreateOnClient))
		{
			Reason = TEXT("component replication requires both creation flags");
		}
		else if (Entry.bReplicateComponent && Owner->GetNetMode() != NM_Standalone && !Owner->GetIsReplicated())
		{
			Reason = TEXT("component replication requires a replicated pawn");
		}
		else if (Entry.bReplicateComponent && Owner->GetNetMode() != NM_Standalone && !ActiveConfig->IsAsset())
		{
			Reason = TEXT("network component replication requires an authored configuration asset");
		}
		if (Reason)
		{
			UE_LOG(LogNelaricPawnInitialization, Error, TEXT("Invalid pawn initialization entry [%d] '%s' on %s: %s."),
			       Index, *Entry.ComponentId.ToString(), *GetNameSafe(Owner), Reason);
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
				UE_LOG(LogNelaricPawnInitialization, Error,
				       TEXT("Invalid pawn initialization entry [%d] '%s' on %s: dependency '%s' %s."), Index,
				       *Entry.ComponentId.ToString(), *GetNameSafe(Owner), *DependencyId.ToString(), Reason);
				return false;
			}
			DeclaredDependencies.Add(DependencyId);
		}
	}
	for (int32 Index = 0; Index < ActiveConfig->Components.Num(); ++Index)
	{
		const FPawnInitializationEntry& Entry = ActiveConfig->Components[Index];
		if (!Nelaric::Pawn::ShouldCreateComponent(Owner, Entry))
		{
			continue;
		}
		const FName InstanceName(*FString::Printf(TEXT("Init_%s"), *Entry.ComponentId.ToString()));
		if (UObject* Existing = FindObject<UObject>(Owner, *InstanceName.ToString()))
		{
			if (!Existing->IsA(Entry.ComponentClass))
			{
				UE_LOG(
				    LogNelaricPawnInitialization, Error,
				    TEXT("Invalid pawn initialization entry [%d] '%s' on %s: instance '%s' has a conflicting class."),
				    Index, *Entry.ComponentId.ToString(), *GetNameSafe(Owner), *InstanceName.ToString());
				return false;
			}
			UE_LOG(LogNelaricPawnInitialization, Error,
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
					    LogNelaricPawnInitialization, Error,
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
	if (bInitializationEnded || bConfiguredComponentsCreated || !bConfigValid || !GetPawn())
	{
		return;
	}
	if (!ActiveConfig)
	{
		bConfigurationValidated = true;
		bConfiguredComponentsCreated = true;
		PublishReplicatedConfiguration();
		return;
	}
	APawn* Owner = GetPawn();
	if (!bConfigurationValidated && !ValidateConfiguration())
	{
		bConfigurationValidated = true;
		bConfigValid = false;
		return;
	}
	bConfigurationValidated = true;
	UWorld* World = GetWorld();
	UInitStateWorldSubsystem* Subsystem = World ? World->GetSubsystem<UInitStateWorldSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogNelaricPawnInitialization, Error,
		       TEXT("Pawn initialization on %s has no init-state world subsystem."), *GetNameSafe(Owner));
		bConfigValid = false;
		return;
	}

	TArray<TWeakObjectPtr<UActorComponent>> ToRegister;
	// Build the entire ID table before resolving any dependencies or registering components.
	for (const FPawnInitializationEntry& Entry : ActiveConfig->Components)
	{
		if (!Nelaric::Pawn::ShouldIncludeComponent(Owner, Entry))
		{
			continue;
		}
		UActorComponent* Component = ConfiguredComponents.FindRef(Entry.ComponentId).Get();
		if (!Component && Entry.bReplicateComponent && !Owner->HasAuthority())
		{
			if (ReplicatedConfiguration.Config == ActiveConfig)
			{
				const FPawnReplicatedInitializationEntry* Binding = ReplicatedConfiguration.Components.FindByPredicate(
				    [&Entry](const FPawnReplicatedInitializationEntry& Candidate)
				    { return Candidate.ComponentId == Entry.ComponentId; });
				Component = Binding ? Binding->Component.Get() : nullptr;
			}
			if (!IsValid(Component))
			{
				continue;
			}
			if (Component->GetOwner() != Owner || !Component->IsA(Entry.ComponentClass) ||
			    !Component->GetIsReplicated())
			{
				bConfigValid = false;
				UE_LOG(LogNelaricPawnInitialization, Error, TEXT("Invalid replicated component binding '%s' on %s."),
				       *Entry.ComponentId.ToString(), *GetNameSafe(Owner));
				return;
			}
			ConfiguredComponents.Add(Entry.ComponentId, Component);
		}
		else if (!Component)
		{
			const FName InstanceName(*FString::Printf(TEXT("Init_%s"), *Entry.ComponentId.ToString()));
			Component = NewObject<UActorComponent>(Owner, Entry.ComponentClass, InstanceName);
			Component->SetIsReplicated(Entry.bReplicateComponent);
			Owner->AddInstanceComponent(Component);
			ConfiguredComponents.Add(Entry.ComponentId, Component);
		}
		if (!Component->IsRegistered())
		{
			ToRegister.Add(Component);
		}
	}
	// Never resolve a partial graph, including optional replicated entries.
	for (const FPawnInitializationEntry& Entry : ActiveConfig->Components)
	{
		if (Nelaric::Pawn::ShouldIncludeComponent(Owner, Entry) &&
		    !IsValid(ConfiguredComponents.FindRef(Entry.ComponentId).Get()))
		{
			return;
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
		if (Entry.bRequiredForPawnReady)
		{
			RequiredComponentIds.Add(Entry.ComponentId);
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
		UE_LOG(LogNelaricPawnInitialization, Error, TEXT("Pawn initialization graph setup failed on %s (config=%s)."),
		       *GetNameSafe(Owner), *GetNameSafe(ActiveConfig));
		bConfigValid = false;
		DestroyConfiguredComponents();
		return;
	}
	bConfiguredComponentsCreated = true;
	PublishReplicatedConfiguration();
	for (const TWeakObjectPtr<UActorComponent>& ComponentPtr : ToRegister)
	{
		if (bInitializationEnded || ActiveConfig != InitializationConfig || !GetPawn())
		{
			bRefreshPending = !bInitializationEnded;
			break;
		}
		if (UActorComponent* Component = ComponentPtr.Get())
		{
			Component->RegisterComponent();
		}
	}
	// Replicas may have registered before their identity references resolved.
	if (!Owner->HasAuthority())
	{
		for (const auto& Pair : ConfiguredComponents)
		{
			if (IsValid(Pair.Value.Get()) && Pair.Value->IsRegistered())
			{
				Subsystem->RegisterParticipant(Pair.Value.Get());
			}
		}
	}
}

void UPawnInitializationComponent::PublishReplicatedConfiguration()
{
	APawn* Owner = GetPawn();
	if (!Owner || !Owner->HasAuthority() || bInitializationEnded)
	{
		return;
	}
	const bool bHasReplicatedEntries =
	    ActiveConfig && ActiveConfig->Components.ContainsByPredicate([](const FPawnInitializationEntry& Entry)
	                                                                 { return Entry.bReplicateComponent; });
	// Preserve independent local configs when component replication is unused.
	if (!bHasReplicatedEntries && ReplicatedConfiguration.Revision == 0)
	{
		return;
	}
	ReplicatedConfiguration.Config = ActiveConfig;
	ReplicatedConfiguration.Components.Reset();
	if (ActiveConfig)
	{
		for (const FPawnInitializationEntry& Entry : ActiveConfig->Components)
		{
			if (Entry.bReplicateComponent)
			{
				ReplicatedConfiguration.Components.Add(
				    {Entry.ComponentId, ConfiguredComponents.FindRef(Entry.ComponentId)});
			}
		}
	}
	++ReplicatedConfiguration.Revision;
	Owner->FlushNetDormancy();
	Owner->ForceNetUpdate();
}

void UPawnInitializationComponent::OnRep_ReplicatedConfiguration()
{
	if (bInitializationEnded)
	{
		return;
	}
	if (bConfiguredComponentsCreated && ActiveConfig == ReplicatedConfiguration.Config)
	{
		for (const FPawnReplicatedInitializationEntry& Entry : ReplicatedConfiguration.Components)
		{
			if (ConfiguredComponents.FindRef(Entry.ComponentId) != Entry.Component)
			{
				RevokePawnReady();
				DestroyConfiguredComponents();
				break;
			}
		}
	}
	SetInitializationConfig(ReplicatedConfiguration.Config);
	if (bInitializationAllowed)
	{
		TryInitializePawn();
	}
}

bool UPawnInitializationComponent::CanRegisterConfiguredParticipant(const UActorComponent* Component) const
{
	const APawn* Owner = GetPawn();
	if (!Owner || Owner->HasAuthority() || !Component)
	{
		return true;
	}
	if (bConfiguredComponentsCreated)
	{
		for (const auto& Pair : ConfiguredComponents)
		{
			if (Pair.Value.Get() == Component)
			{
				return true;
			}
		}
	}
	const UPawnInitializationConfig* Config = InitializationConfig;
	bool bMatchesReplicatedEntry = false;
	if (Config)
	{
		for (const FPawnInitializationEntry& Entry : Config->Components)
		{
			if (Entry.bReplicateComponent && Entry.ComponentClass && Component->IsA(Entry.ComponentClass))
			{
				bMatchesReplicatedEntry = true;
				if (bConfiguredComponentsCreated && IsConfiguredInstance(Entry.ComponentId, Component))
				{
					return true;
				}
			}
		}
	}
	return !bMatchesReplicatedEntry;
}

void UPawnInitializationComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bInitializationAllowed && !bInitializationEnded && !bConfiguredComponentsCreated)
	{
		TryInitializePawn();
	}
}

void UPawnInitializationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UPawnInitializationComponent, ReplicatedConfiguration);
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
	if ((ActiveConfig && !Subsystem) || (Subsystem && (Subsystem->bInvalidatingDependents || Subsystem->bShuttingDown)))
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
