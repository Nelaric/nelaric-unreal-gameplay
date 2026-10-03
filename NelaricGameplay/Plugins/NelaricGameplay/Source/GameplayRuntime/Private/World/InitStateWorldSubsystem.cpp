// Copyright (c) 2026 Nelaric Contributors

#include "Pawn/InitStateWorldSubsystem.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Pawn/PawnInitializationComponent.h"
#include "Pawn/InitStateParticipantInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogNelaricInitGraph, Log, All);

bool UInitStateWorldSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UInitStateWorldSubsystem::RegisterParticipant(UActorComponent* Component)
{
	if (bShuttingDown || !IsValid(Component) || StoppedComponents.Contains(Component) ||
	    Component->GetWorld() != GetWorld() ||
	    !Component->GetClass()->ImplementsInterface(UInitStateParticipantInterface::StaticClass()))
	{
		return;
	}
	if (AActor* Owner = Component->GetOwner())
	{
		TArray<UPawnInitializationComponent*> Managers;
		Owner->GetComponents(Managers);
		for (const UPawnInitializationComponent* Manager : Managers)
		{
			if (IsValid(Manager) && !Manager->CanRegisterConfiguredParticipant(Component))
			{
				return;
			}
		}
	}
	static const FString ManagedPrefix(TEXT("Init_"));
	const FString ComponentName = Component->GetName();
	if (ComponentName.StartsWith(ManagedPrefix))
	{
		const FName ComponentId(*ComponentName.RightChop(ManagedPrefix.Len()));
		if (AActor* Owner = Component->GetOwner())
		{
			TArray<UPawnInitializationComponent*> Managers;
			Owner->GetComponents<UPawnInitializationComponent>(Managers);
			for (const UPawnInitializationComponent* Manager : Managers)
			{
				if (IsValid(Manager) && Manager->HasConfiguredId(ComponentId) &&
				    !Manager->IsConfiguredInstance(ComponentId, Component))
				{
					return;
				}
			}
		}
	}

	RegisteredComponents.Add(Component);
	ObservedApplicability.Add(Component, Cast<IInitStateParticipantInterface>(Component)->IsInitApplicable());
	QueueParticipantAndDependents(Component);
	ProcessParticipants();
}

void UInitStateWorldSubsystem::UnregisterParticipant(UActorComponent* Component)
{
	RegisteredComponents.Remove(Component);
	ObservedApplicability.Remove(Component);
	if (bShuttingDown || StoppedComponents.Contains(Component))
	{
		return;
	}
	if (IsValid(Component))
	{
		PendingOwners.Add(Component->GetOwner());
	}
	InvalidateConfiguredDependents(Component);
	QueueParticipantAndDependents(Component);
	ProcessParticipants();
}

bool UInitStateWorldSubsystem::ConfigureParticipant(UActorComponent* Component, FName ComponentId,
                                                    bool bRequiredForPawnReady,
                                                    const TArray<UActorComponent*>& Dependencies)
{
	Nelaric::FInitParticipantConfiguration Configuration;
	Configuration.Component = Component;
	Configuration.ComponentId = ComponentId;
	Configuration.bRequiredForPawnReady = bRequiredForPawnReady;
	Configuration.Dependencies = Dependencies;
	return ConfigureParticipants({Configuration});
}

bool UInitStateWorldSubsystem::ConfigureParticipants(
    const TArray<Nelaric::FInitParticipantConfiguration>& Configurations)
{
	if (bShuttingDown)
	{
		UE_LOG(LogNelaricInitGraph, Error,
		       TEXT("Cannot configure initialization participants: subsystem is shutting down."));
		return false;
	}
	for (int32 Index = 0; Index < Configurations.Num(); ++Index)
	{
		const Nelaric::FInitParticipantConfiguration& Entry = Configurations[Index];
		UActorComponent* Component = Entry.Component;
		AActor* Owner = IsValid(Component) ? Component->GetOwner() : nullptr;
		if (!Owner || Component->GetWorld() != GetWorld() || Entry.ComponentId.IsNone() ||
		    StoppedComponents.Contains(Component) || ConfiguredComponents.Contains(Component))
		{
			UE_LOG(LogNelaricInitGraph, Error,
			       TEXT("Cannot configure initialization participant: component=%s id=%s owner=%s; invalid world, ID "
			            "or registration."),
			       *GetNameSafe(Component), *Entry.ComponentId.ToString(), *GetNameSafe(Owner));
			return false;
		}
		for (int32 PreviousIndex = 0; PreviousIndex < Index; ++PreviousIndex)
		{
			const Nelaric::FInitParticipantConfiguration& Previous = Configurations[PreviousIndex];
			if (Previous.Component == Component || (Previous.Component && Previous.Component->GetOwner() == Owner &&
			                                        Previous.ComponentId == Entry.ComponentId))
			{
				UE_LOG(LogNelaricInitGraph, Error,
				       TEXT("Cannot configure initialization participants: duplicate component or ID within the "
				            "configuration batch."));
				return false;
			}
		}
		for (const auto& Pair : ConfiguredComponents)
		{
			if (Pair.Value.Owner.Get() == Owner && Pair.Value.ComponentId == Entry.ComponentId &&
			    Pair.Key.Get() != Component)
			{
				UE_LOG(LogNelaricInitGraph, Error,
				       TEXT("Cannot configure initialization participants: component ID is already configured for this "
				            "owner."));
				return false;
			}
		}
		TArray<UPawnInitializationComponent*> Managers;
		Owner->GetComponents<UPawnInitializationComponent>(Managers);
		for (const UPawnInitializationComponent* Manager : Managers)
		{
			if (IsValid(Manager) && Manager->HasConfiguredId(Entry.ComponentId) &&
			    !Manager->IsConfiguredInstance(Entry.ComponentId, Component))
			{
				UE_LOG(LogNelaricInitGraph, Error,
				       TEXT("Cannot configure initialization participants: component conflicts with a managed "
				            "initialization instance."));
				return false;
			}
		}
	}
	bool bChanged = false;
	for (const Nelaric::FInitParticipantConfiguration& Entry : Configurations)
	{
		UActorComponent* Component = Entry.Component;
		if (!IsValid(Component) || Entry.ComponentId.IsNone())
		{
			continue;
		}
		FConfiguredParticipant Configuration;
		Configuration.Owner = Component->GetOwner();
		Configuration.ComponentId = Entry.ComponentId;
		Configuration.bRequiredForPawnReady = Entry.bRequiredForPawnReady;
		for (UActorComponent* Dependency : Entry.Dependencies)
		{
			Configuration.Dependencies.Add(Dependency);
		}
		ConfiguredComponents.Add(Component, MoveTemp(Configuration));
		PendingOwners.Add(Component->GetOwner());
		bChanged = true;
	}
	if (bChanged)
	{
		RebuildDependencyGraph();
		QueueAllRegistered();
		ProcessParticipants();
	}
	return true;
}

void UInitStateWorldSubsystem::StopConfiguredParticipants(const TArray<UActorComponent*>& Components)
{
	bool bChanged = false;
	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}
		StoppedComponents.Add(Component);
		RegisteredComponents.Remove(Component);
		ObservedApplicability.Remove(Component);
		PendingInvalidationRoots.Remove(Component);
		QueuedComponents.Remove(Component);
		bChanged |= ConfiguredComponents.Remove(Component) > 0;
		if (IsValid(Component))
		{
			PendingOwners.Remove(Component->GetOwner());
		}
	}
	if (!bProcessing)
	{
		PendingQueue.RemoveAll([this](const FComponentPtr& Component)
		                       { return StoppedComponents.Contains(Component); });
	}
	if (bChanged && !bShuttingDown)
	{
		RebuildDependencyGraph();
		QueueAllRegistered();
		ProcessParticipants();
	}
}

bool UInitStateWorldSubsystem::HasConfiguredId(const AActor* Owner, FName ComponentId) const
{
	for (const auto& Pair : ConfiguredComponents)
	{
		if (Pair.Value.Owner.Get() == Owner && Pair.Value.ComponentId == ComponentId)
		{
			return true;
		}
	}
	return false;
}

void UInitStateWorldSubsystem::RebuildDependencyGraph()
{
	++GraphVersion;
	ForwardDependencies.Empty();
	ReverseDependencies.Empty();
	RequiredByOwner.Empty();
	ReadyGroupByComponent.Empty();
	ReadyGroups.Empty();
	GraphEdgeCount = 0;
	for (const auto& Pair : ConfiguredComponents)
	{
		if (Pair.Value.bRequiredForPawnReady)
		{
			RequiredByOwner.FindOrAdd(Pair.Value.Owner).Add(Pair.Key);
		}
		if (!IsValid(Pair.Key.Get()))
		{
			continue;
		}
		TArray<FComponentPtr>& Forward = ForwardDependencies.FindOrAdd(Pair.Key);
		for (const FComponentPtr& Dependency : Pair.Value.Dependencies)
		{
			Forward.AddUnique(Dependency);
			if (IsValid(Dependency.Get()))
			{
				ReverseDependencies.FindOrAdd(Dependency).AddUnique(Pair.Key);
			}
		}
		GraphEdgeCount += Forward.Num();
	}

	struct FTraversalFrame
	{
		FComponentPtr Component;
		int32 NextDependency = 0;
	};
	TSet<FComponentPtr> Visited;
	TArray<FComponentPtr> FinishOrder;
	for (const auto& Pair : ForwardDependencies)
	{
		if (Visited.Contains(Pair.Key))
		{
			continue;
		}
		Visited.Add(Pair.Key);
		TArray<FTraversalFrame> Stack{{Pair.Key, 0}};
		while (!Stack.IsEmpty())
		{
			FTraversalFrame& Frame = Stack.Last();
			const TArray<FComponentPtr>& Dependencies = ForwardDependencies.FindChecked(Frame.Component);
			if (Frame.NextDependency < Dependencies.Num())
			{
				const FComponentPtr Dependency = Dependencies[Frame.NextDependency++];
				if (ForwardDependencies.Contains(Dependency) && !Visited.Contains(Dependency))
				{
					Visited.Add(Dependency);
					Stack.Add({Dependency, 0});
				}
			}
			else
			{
				FinishOrder.Add(Frame.Component);
				Stack.Pop();
			}
		}
	}
	for (int32 Index = FinishOrder.Num() - 1; Index >= 0; --Index)
	{
		const FComponentPtr Start = FinishOrder[Index];
		if (ReadyGroupByComponent.Contains(Start))
		{
			continue;
		}
		const int32 GroupIndex = ReadyGroups.AddDefaulted();
		TArray<FComponentPtr> Stack{Start};
		ReadyGroupByComponent.Add(Start, GroupIndex);
		while (!Stack.IsEmpty())
		{
			const FComponentPtr Current = Stack.Pop();
			ReadyGroups[GroupIndex].Add(Current);
			if (const TArray<FComponentPtr>* Dependents = ReverseDependencies.Find(Current))
			{
				for (const FComponentPtr& Dependent : *Dependents)
				{
					if (ForwardDependencies.Contains(Dependent) && !ReadyGroupByComponent.Contains(Dependent))
					{
						ReadyGroupByComponent.Add(Dependent, GroupIndex);
						Stack.Add(Dependent);
					}
				}
			}
		}
	}
}

void UInitStateWorldSubsystem::QueueParticipant(UActorComponent* Component)
{
	if (!bShuttingDown && IsValid(Component) && !StoppedComponents.Contains(Component) &&
	    RegisteredComponents.Contains(Component) && !QueuedComponents.Contains(Component))
	{
		QueuedComponents.Add(Component);
		PendingQueue.Add(Component);
		PendingOwners.Add(Component->GetOwner());
	}
}

void UInitStateWorldSubsystem::QueueParticipantAndDependents(UActorComponent* Component)
{
	QueueParticipant(Component);
	if (const TArray<FComponentPtr>* Dependents = ReverseDependencies.Find(Component))
	{
		for (const FComponentPtr& Dependent : *Dependents)
		{
			QueueParticipant(Dependent.Get());
		}
	}
}

void UInitStateWorldSubsystem::QueueAllRegistered()
{
	for (const FComponentPtr& Component : RegisteredComponents)
	{
		QueueParticipant(Component.Get());
	}
}

bool UInitStateWorldSubsystem::AreRequiredParticipantsReady(const AActor* Owner) const
{
	if (bShuttingDown || !IsValid(Owner))
	{
		return false;
	}
	const TArray<FComponentPtr>* RequiredComponents = RequiredByOwner.Find(Owner);
	if (RequiredComponents)
	{
		for (const FComponentPtr& Component : *RequiredComponents)
		{
			if (!IsParticipantReady(Component.Get()))
			{
				return false;
			}
		}
	}
	return true;
}

bool UInitStateWorldSubsystem::IsParticipantReady(UActorComponent* Component) const
{
	if (bShuttingDown || !IsValid(Component) || !ConfiguredComponents.Contains(Component))
	{
		return false;
	}
	TArray<FComponentPtr> Pending{Component};
	TSet<FComponentPtr> Visited;
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		const FComponentPtr Current = Pending[Index];
		if (Visited.Contains(Current))
		{
			continue;
		}
		Visited.Add(Current);
		UActorComponent* Member = Current.Get();
		const IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Member);
		if (!IsValid(Member) || Member->GetOwner() != Component->GetOwner() ||
		    !RegisteredComponents.Contains(Current) || StoppedComponents.Contains(Current) || !Participant ||
		    !Participant->IsInitApplicable() || Participant->HasTerminalInitFailure() ||
		    Participant->GetInitState() != Nelaric::EInitState::Ready)
		{
			return false;
		}
		if (const TArray<FComponentPtr>* Dependencies = ForwardDependencies.Find(Current))
		{
			Pending.Append(*Dependencies);
		}
	}
	return true;
}

void UInitStateWorldSubsystem::InvalidateParticipantContext(UActorComponent* Component)
{
	InvalidateParticipants({Component}, true);
	ProcessParticipants();
}

void UInitStateWorldSubsystem::InvalidateActorContext(AActor* Owner)
{
	TArray<FComponentPtr> Roots;
	for (const FComponentPtr& Component : RegisteredComponents)
	{
		if (Component.IsValid() && Component->GetOwner() == Owner)
		{
			Roots.Add(Component);
		}
	}
	InvalidateParticipants(Roots, true);
	ProcessParticipants();
}

void UInitStateWorldSubsystem::InvalidateConfiguredDependents(UActorComponent* Component)
{
	InvalidateParticipants({Component}, false);
}

void UInitStateWorldSubsystem::InvalidateParticipants(const TArray<FComponentPtr>& Roots, bool bIncludeRoots)
{
	if (bShuttingDown)
	{
		return;
	}
	// Queue nested invalidations instead of interrupting the cleanup batch.
	if (bInvalidatingDependents)
	{
		if (bIncludeRoots)
		{
			for (const FComponentPtr& Root : Roots)
			{
				PendingInvalidationRoots.Add(Root);
			}
		}
		else
		{
			for (const FComponentPtr& Root : Roots)
			{
				if (!ActiveInvalidationMembers.Contains(Root))
				{
					for (const FComponentPtr& Dependent : ReverseDependencies.FindRef(Root))
					{
						PendingInvalidationRoots.Add(Dependent);
					}
				}
			}
		}
		return;
	}
	bInvalidatingDependents = true;
	ON_SCOPE_EXIT
	{
		bInvalidatingDependents = false;
		ActiveInvalidationMembers.Empty();
	};
	TSet<FComponentPtr> Visited;
	TArray<FComponentPtr> Pending = Roots;
	TArray<FComponentPtr> ToInvalidate;
	TSet<TWeakObjectPtr<AActor>> Owners;
	for (const FComponentPtr& Root : Roots)
	{
		Visited.Add(Root);
		if (Root.IsValid())
		{
			Owners.Add(Root->GetOwner());
			if (bIncludeRoots)
			{
				ToInvalidate.Add(Root);
			}
		}
	}
	// Snapshot the traversal before cleanup callbacks can rebuild the graph.
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		if (const TArray<FComponentPtr>* Dependents = ReverseDependencies.Find(Pending[Index]))
		{
			for (const FComponentPtr& Dependent : *Dependents)
			{
				if (!Visited.Contains(Dependent))
				{
					Visited.Add(Dependent);
					Pending.Add(Dependent);
					ToInvalidate.Add(Dependent);
					if (Dependent.IsValid())
					{
						Owners.Add(Dependent->GetOwner());
					}
				}
			}
		}
	}
	ActiveInvalidationMembers = Visited;
	TMap<FComponentPtr, Nelaric::FInitGeneration> Generations;
	for (const FComponentPtr& Member : ToInvalidate)
	{
		if (const IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Member.Get()))
		{
			Generations.Add(Member, Participant->GetInitGeneration());
			ObservedApplicability.Add(Member, Participant->IsInitApplicable());
		}
	}
	// Stop pawn-level gameplay before participant cleanup callbacks run.
	for (const TWeakObjectPtr<AActor>& OwnerPtr : Owners)
	{
		if (AActor* Owner = OwnerPtr.Get())
		{
			TArray<UPawnInitializationComponent*> Managers;
			Owner->GetComponents<UPawnInitializationComponent>(Managers);
			for (UPawnInitializationComponent* Manager : Managers)
			{
				if (IsValid(Manager))
				{
					Manager->RevokePawnReady();
				}
			}
		}
	}
	for (const FComponentPtr& Member : ToInvalidate)
	{
		if (bShuttingDown)
		{
			break;
		}
		UActorComponent* Component = Member.Get();
		IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Component);
		const Nelaric::FInitGeneration* Generation = Generations.Find(Member);
		if (Participant && Generation && *Generation == Participant->GetInitGeneration() &&
		    RegisteredComponents.Contains(Member) && !StoppedComponents.Contains(Member))
		{
			Participant->InvalidateInitGeneration();
			QueueParticipant(Component);
		}
	}
	for (const TWeakObjectPtr<AActor>& Owner : Owners)
	{
		PendingOwners.Add(Owner);
	}
}

bool UInitStateWorldSubsystem::TryCommitReadyGroup(UActorComponent* Root)
{
	TArray<FComponentPtr> Pending{Root};
	if (const int32* GroupIndex = ReadyGroupByComponent.Find(Root))
	{
		Pending = ReadyGroups[*GroupIndex];
	}
	const uint64 ExpectedGraphVersion = GraphVersion;
	TMap<FComponentPtr, Nelaric::FInitStateSnapshot> BeforeReady;
	TMap<FComponentPtr, Nelaric::FInitStateSnapshot> DependenciesBefore;
	const TWeakObjectPtr<AActor> ExpectedOwner = Root->GetOwner();
	auto Capture = [this, &ExpectedOwner](const FComponentPtr& Member, Nelaric::EInitState State,
	                                      TMap<FComponentPtr, Nelaric::FInitStateSnapshot>& Snapshots)
	{
		UActorComponent* Component = Member.Get();
		const IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Component);
		if (!Component || Component->GetWorld() != GetWorld() || Component->GetOwner() != ExpectedOwner.Get() ||
		    !RegisteredComponents.Contains(Member) || StoppedComponents.Contains(Member) || !Participant ||
		    !Participant->IsInitApplicable() || Participant->HasTerminalInitFailure() ||
		    Participant->GetInitState() != State)
		{
			return false;
		}
		Snapshots.Add(Member, {Participant->GetInitGeneration(), State, false});
		return true;
	};
	for (const FComponentPtr& Member : Pending)
	{
		if (!Capture(Member, Nelaric::EInitState::DataInitialized, BeforeReady))
		{
			return false;
		}
		// Copy edges before invoking participant code.
		const TArray<FComponentPtr> Dependencies = ForwardDependencies.FindRef(Member);
		for (const FComponentPtr& Dependency : Dependencies)
		{
			if (!Pending.Contains(Dependency) && !Capture(Dependency, Nelaric::EInitState::Ready, DependenciesBefore))
			{
				return false;
			}
		}
	}
	// Recheck the captured rounds and external dependencies after callbacks.
	auto Validate = [this, ExpectedGraphVersion, &ExpectedOwner, &BeforeReady, &DependenciesBefore](bool bCommitted)
	{
		if (bShuttingDown || GraphVersion != ExpectedGraphVersion || !ExpectedOwner.IsValid())
		{
			return false;
		}
		TArray<UPawnInitializationComponent*> Managers;
		ExpectedOwner->GetComponents<UPawnInitializationComponent>(Managers);
		for (const UPawnInitializationComponent* Manager : Managers)
		{
			if (IsValid(Manager) && (Manager->bInitializationEnded || Manager->bContextResetRequested ||
			                         Manager->ActiveConfig != Manager->InitializationConfig))
			{
				return false;
			}
		}
		for (const auto& Pair : BeforeReady)
		{
			UActorComponent* Component = Pair.Key.Get();
			const IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Component);
			if (!Component || !RegisteredComponents.Contains(Pair.Key) || StoppedComponents.Contains(Pair.Key) ||
			    Component->GetOwner() != ExpectedOwner.Get() || Component->GetWorld() != GetWorld() || !Participant ||
			    !Participant->IsInitApplicable() || Participant->HasTerminalInitFailure() ||
			    !(Pair.Value.Generation == Participant->GetInitGeneration()) ||
			    Participant->GetInitState() !=
			        (bCommitted ? Nelaric::EInitState::Ready : Nelaric::EInitState::DataInitialized))
			{
				return false;
			}
		}
		for (const auto& Pair : DependenciesBefore)
		{
			const IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Pair.Key.Get());
			if (!Participant || !IsParticipantReady(Pair.Key.Get()) ||
			    !(Pair.Value.Generation == Participant->GetInitGeneration()))
			{
				return false;
			}
		}
		return !bShuttingDown && GraphVersion == ExpectedGraphVersion;
	};
	for (const FComponentPtr& Member : Pending)
	{
		if (!Validate(false))
		{
			return false;
		}
		IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Member.Get());
		if (!Participant->CanEnterReady() || !Validate(false))
		{
			return false;
		}
	}
	// No user callbacks occur between this final validation and silent commits.
	if (!Validate(false))
	{
		return false;
	}
	for (const FComponentPtr& Member : Pending)
	{
		IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Member.Get());
		if (!ensureMsgf(Participant->CommitReadyWithoutNotification(),
		                TEXT("Ready group member failed its silent commit.")))
		{
			InvalidateParticipants(Pending, true);
			return false;
		}
	}
	for (const FComponentPtr& Member : Pending)
	{
		if (!Validate(true))
		{
			break;
		}
		Cast<IInitStateParticipantInterface>(Member.Get())->NotifyReadyCommitted(BeforeReady.FindChecked(Member));
	}
	// Reset only surviving old rounds; preserve failures and newer attempts.
	if (!Validate(true))
	{
		TArray<FComponentPtr> OldRoundMembers;
		for (const auto& Pair : BeforeReady)
		{
			const IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Pair.Key.Get());
			if (Participant && !Participant->HasTerminalInitFailure() &&
			    Pair.Value.Generation == Participant->GetInitGeneration() &&
			    Participant->GetInitState() == Nelaric::EInitState::Ready)
			{
				OldRoundMembers.Add(Pair.Key);
			}
		}
		InvalidateParticipants(OldRoundMembers, true);
	}
	return true;
}

void UInitStateWorldSubsystem::NotifyParticipantChanged(UActorComponent* Component,
                                                        const Nelaric::FInitStateSnapshot& Previous)
{
	if (bShuttingDown || StoppedComponents.Contains(Component) || !RegisteredComponents.Contains(Component))
	{
		return;
	}

	const IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Component);
	if (!Participant)
	{
		return;
	}

	const bool bChanged = !(Previous.Generation == Participant->GetInitGeneration()) ||
	                      Previous.State != Participant->GetInitState() ||
	                      Previous.bTerminallyFailed != Participant->HasTerminalInitFailure();
	if (bChanged)
	{
		if (!(Previous.Generation == Participant->GetInitGeneration()) ||
		    (!Previous.bTerminallyFailed && Participant->HasTerminalInitFailure()) ||
		    (Previous.State == Nelaric::EInitState::Ready && Participant->GetInitState() != Nelaric::EInitState::Ready))
		{
			InvalidateConfiguredDependents(Component);
		}
		QueueParticipant(Component);
		if (Previous.State == Nelaric::EInitState::Ready || Participant->GetInitState() == Nelaric::EInitState::Ready)
		{
			QueueParticipantAndDependents(Component);
		}
		ProcessParticipants();
	}
}

void UInitStateWorldSubsystem::RequestParticipantRefresh(UActorComponent* Component)
{
	if (!bShuttingDown && !StoppedComponents.Contains(Component) && RegisteredComponents.Contains(Component))
	{
		QueueParticipant(Component);
		ProcessParticipants();
	}
}

void UInitStateWorldSubsystem::ProcessParticipants()
{
	if (bShuttingDown || bProcessing || bInvalidatingDependents)
	{
		return;
	}

	bProcessing = true;
	ON_SCOPE_EXIT
	{
		bProcessing = false;
	};

	const int32 MaxSteps = FMath::Max(64, (RegisteredComponents.Num() + GraphEdgeCount) * 8 + PendingQueue.Num() * 4);
	int32 Steps = 0;
	while (!bShuttingDown &&
	       (!PendingQueue.IsEmpty() || !PendingOwners.IsEmpty() || !PendingInvalidationRoots.IsEmpty()) &&
	       Steps < MaxSteps)
	{
		if (!PendingInvalidationRoots.IsEmpty())
		{
			const TArray<FComponentPtr> Roots = PendingInvalidationRoots.Array();
			PendingInvalidationRoots.Empty();
			InvalidateParticipants(Roots, true);
			++Steps;
		}
		// Callbacks can change the pending queue without invalidating this batch.
		TArray<FComponentPtr> Batch = MoveTemp(PendingQueue);
		PendingQueue.Empty();
		for (int32 Index = 0; Index < Batch.Num(); ++Index)
		{
			const FComponentPtr ParticipantPtr = Batch[Index];
			if (bShuttingDown || Steps >= MaxSteps)
			{
				if (!bShuttingDown)
				{
					PendingQueue.Append(Batch.GetData() + Index, Batch.Num() - Index);
				}
				break;
			}
			QueuedComponents.Remove(ParticipantPtr);
			++Steps;
			UActorComponent* Component = ParticipantPtr.Get();
			if (!IsValid(Component) || Component->GetWorld() != GetWorld())
			{
				RegisteredComponents.Remove(ParticipantPtr);
				continue;
			}
			if (StoppedComponents.Contains(ParticipantPtr) || !RegisteredComponents.Contains(ParticipantPtr))
			{
				continue;
			}

			IInitStateParticipantInterface* Participant = Cast<IInitStateParticipantInterface>(Component);
			if (!Participant)
			{
				continue;
			}
			const bool bApplicable = Participant->IsInitApplicable();
			bool& bObservedApplicable = ObservedApplicability.FindOrAdd(ParticipantPtr, bApplicable);
			// Participation changes invalidate old preparation and dependent work.
			if (bObservedApplicable != bApplicable)
			{
				bObservedApplicable = bApplicable;
				InvalidateParticipants({ParticipantPtr}, true);
				continue;
			}
			if (!bApplicable || Participant->HasTerminalInitFailure())
			{
				continue;
			}
			if (Participant->GetInitState() == Nelaric::EInitState::Ready)
			{
				if (ConfiguredComponents.Contains(ParticipantPtr) && !IsParticipantReady(Component))
				{
					InvalidateParticipants({ParticipantPtr}, true);
				}
				continue;
			}

			const Nelaric::FInitGeneration Generation = Participant->GetInitGeneration();
			const Nelaric::EInitState PreviousState = Participant->GetInitState();
			bool bAdvanced = false;
			if (PreviousState == Nelaric::EInitState::DataInitialized)
			{
				bAdvanced = TryCommitReadyGroup(Component);
			}
			else
			{
				bAdvanced = Participant->TryChangeInitState();
			}
			// Callbacks may remove the component or invalidate its committed round.
			Component = ParticipantPtr.Get();
			Participant = Cast<IInitStateParticipantInterface>(Component);
			if (!Participant || !RegisteredComponents.Contains(ParticipantPtr) ||
			    !(Generation == Participant->GetInitGeneration()))
			{
				continue;
			}
			const bool bExactlyOneStep =
			    static_cast<uint8>(Participant->GetInitState()) == static_cast<uint8>(PreviousState) + 1;
			if (bAdvanced && bExactlyOneStep)
			{
				QueueParticipantAndDependents(Component);
			}
		}
		const TArray<TWeakObjectPtr<AActor>> Owners = PendingOwners.Array();
		PendingOwners.Empty();
		for (const TWeakObjectPtr<AActor>& OwnerPtr : Owners)
		{
			if (bShuttingDown)
			{
				break;
			}
			if (AActor* Owner = OwnerPtr.Get())
			{
				TArray<UPawnInitializationComponent*> Managers;
				Owner->GetComponents<UPawnInitializationComponent>(Managers);
				for (UPawnInitializationComponent* Manager : Managers)
				{
					if (IsValid(Manager))
					{
						Manager->TryInitializePawn();
					}
				}
			}
		}
	}
	ensureMsgf(PendingQueue.IsEmpty(), TEXT("Initialization participants requested too many progress steps."));
}

void UInitStateWorldSubsystem::Deinitialize()
{
	bShuttingDown = true;
	RegisteredComponents.Empty();
	ObservedApplicability.Empty();
	PendingInvalidationRoots.Empty();
	ActiveInvalidationMembers.Empty();
	StoppedComponents.Empty();
	ConfiguredComponents.Empty();
	ForwardDependencies.Empty();
	ReverseDependencies.Empty();
	RequiredByOwner.Empty();
	ReadyGroupByComponent.Empty();
	ReadyGroups.Empty();
	GraphEdgeCount = 0;
	PendingQueue.Empty();
	QueuedComponents.Empty();
	PendingOwners.Empty();
	Super::Deinitialize();
}
