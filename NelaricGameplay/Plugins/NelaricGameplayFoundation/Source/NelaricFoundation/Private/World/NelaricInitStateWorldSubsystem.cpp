// Copyright (c) 2026 Nelaric

#include "World/NelaricInitStateWorldSubsystem.h"

#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopeExit.h"
#include "Pawn/NelaricPawnInitializationComponent.h"
#include "World/NelaricInitStateParticipantInterface.h"

void UNelaricInitStateWorldSubsystem::RegisterParticipant(UActorComponent* Component)
{
	if (!IsValid(Component) || Component->GetWorld() != GetWorld() ||
	    !Component->GetClass()->ImplementsInterface(UNelaricInitStateParticipantInterface::StaticClass()))
	{
		return;
	}

	RegisteredComponents.Add(Component);
	QueueParticipantAndDependents(Component);
	ProcessParticipants();
}

void UNelaricInitStateWorldSubsystem::UnregisterParticipant(UActorComponent* Component)
{
	RegisteredComponents.Remove(Component);
	if (IsValid(Component))
	{
		PendingOwners.Add(Component->GetOwner());
	}
	InvalidateConfiguredDependents(Component);
	QueueParticipantAndDependents(Component);
	ProcessParticipants();
}

void UNelaricInitStateWorldSubsystem::ConfigureParticipant(UActorComponent* Component, FName ComponentId,
                                                           bool bRequiredForPawnReady,
                                                           const TArray<UActorComponent*>& Dependencies)
{
	Nelaric::FInitParticipantConfiguration Configuration;
	Configuration.Component = Component;
	Configuration.ComponentId = ComponentId;
	Configuration.bRequiredForPawnReady = bRequiredForPawnReady;
	Configuration.Dependencies = Dependencies;
	ConfigureParticipants({Configuration});
}

void UNelaricInitStateWorldSubsystem::ConfigureParticipants(
    const TArray<Nelaric::FInitParticipantConfiguration>& Configurations)
{
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
}

void UNelaricInitStateWorldSubsystem::UnconfigureParticipant(UActorComponent* Component)
{
	if (ConfiguredComponents.Remove(Component) > 0)
	{
		if (IsValid(Component))
		{
			PendingOwners.Add(Component->GetOwner());
		}
		RebuildDependencyGraph();
		QueueAllRegistered();
		ProcessParticipants();
	}
}

void UNelaricInitStateWorldSubsystem::RebuildDependencyGraph()
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

void UNelaricInitStateWorldSubsystem::QueueParticipant(UActorComponent* Component)
{
	if (IsValid(Component) && RegisteredComponents.Contains(Component) && !QueuedComponents.Contains(Component))
	{
		QueuedComponents.Add(Component);
		PendingQueue.Add(Component);
		PendingOwners.Add(Component->GetOwner());
	}
}

void UNelaricInitStateWorldSubsystem::QueueParticipantAndDependents(UActorComponent* Component)
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

void UNelaricInitStateWorldSubsystem::QueueAllRegistered()
{
	for (const FComponentPtr& Component : RegisteredComponents)
	{
		QueueParticipant(Component.Get());
	}
}

bool UNelaricInitStateWorldSubsystem::AreRequiredParticipantsReady(const AActor* Owner) const
{
	if (!Owner)
	{
		return false;
	}
	const TArray<FComponentPtr>* RequiredComponents = RequiredByOwner.Find(Owner);
	if (!RequiredComponents)
	{
		return true;
	}
	for (const FComponentPtr& ComponentPtr : *RequiredComponents)
	{
		UActorComponent* Component = ComponentPtr.Get();
		const INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Component);
		if (!IsValid(Component) || !RegisteredComponents.Contains(Component) || !Participant ||
		    !Participant->IsInitApplicable() || Participant->HasTerminalInitFailure() ||
		    Participant->GetInitState() != Nelaric::EInitState::Ready)
		{
			return false;
		}
	}
	return true;
}

bool UNelaricInitStateWorldSubsystem::IsParticipantReady(UActorComponent* Component) const
{
	const INelaricInitStateParticipantInterface* Participant = Cast<INelaricInitStateParticipantInterface>(Component);
	return IsValid(Component) && ConfiguredComponents.Contains(Component) && RegisteredComponents.Contains(Component) &&
	       Participant && Participant->IsInitApplicable() && !Participant->HasTerminalInitFailure() &&
	       Participant->GetInitState() == Nelaric::EInitState::Ready;
}

void UNelaricInitStateWorldSubsystem::InvalidateConfiguredDependents(UActorComponent* Component)
{
	if (!IsValid(Component) || bInvalidatingDependents)
	{
		return;
	}
	bInvalidatingDependents = true;
	ON_SCOPE_EXIT
	{
		bInvalidatingDependents = false;
	};
	TSet<FComponentPtr> Visited{Component};
	TArray<FComponentPtr> Pending{Component};
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		const TArray<FComponentPtr>* Dependents = ReverseDependencies.Find(Pending[Index]);
		if (!Dependents)
		{
			continue;
		}
		for (const FComponentPtr& DependentPtr : *Dependents)
		{
			if (Visited.Contains(DependentPtr))
			{
				continue;
			}
			Visited.Add(DependentPtr);
			Pending.Add(DependentPtr);
			INelaricInitStateParticipantInterface* Dependent =
			    Cast<INelaricInitStateParticipantInterface>(DependentPtr.Get());
			if (Dependent && Dependent->GetInitState() == Nelaric::EInitState::Ready)
			{
				Dependent->InvalidateInitGeneration();
			}
		}
	}
}

bool UNelaricInitStateWorldSubsystem::TryCommitReadyGroup(UActorComponent* Root)
{
	TArray<UActorComponent*> Pending{Root};
	const int32* FoundGroupIndex = ReadyGroupByComponent.Find(Root);
	const int32 RootGroupIndex = FoundGroupIndex ? *FoundGroupIndex : INDEX_NONE;
	if (RootGroupIndex != INDEX_NONE)
	{
		Pending.Empty();
		for (const FComponentPtr& Member : ReadyGroups[RootGroupIndex])
		{
			Pending.Add(Member.Get());
		}
	}
	const uint64 ExpectedGraphVersion = GraphVersion;
	for (UActorComponent* Component : Pending)
	{
		if (GraphVersion != ExpectedGraphVersion || !IsValid(Component) || Component->GetWorld() != GetWorld() ||
		    !RegisteredComponents.Contains(Component))
		{
			return false;
		}
		const INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Component);
		if (!Participant || !Participant->IsInitApplicable() || Participant->HasTerminalInitFailure() ||
		    Participant->GetInitState() != Nelaric::EInitState::DataInitialized || GraphVersion != ExpectedGraphVersion)
		{
			return false;
		}
		if (const TArray<FComponentPtr>* Dependencies = ForwardDependencies.Find(Component))
		{
			for (const FComponentPtr& DependencyPtr : *Dependencies)
			{
				UActorComponent* Dependency = DependencyPtr.Get();
				const INelaricInitStateParticipantInterface* Required =
				    Cast<INelaricInitStateParticipantInterface>(Dependency);
				if (!IsValid(Dependency) || Dependency->GetOwner() != Component->GetOwner() || !Required ||
				    !RegisteredComponents.Contains(Dependency) || !Required->IsInitApplicable() ||
				    Required->HasTerminalInitFailure() || GraphVersion != ExpectedGraphVersion)
				{
					return false;
				}
				const int32* DependencyGroupIndex = ReadyGroupByComponent.Find(Dependency);
				const bool bInGroup =
				    RootGroupIndex != INDEX_NONE && DependencyGroupIndex && RootGroupIndex == *DependencyGroupIndex;
				if (!bInGroup && Required->GetInitState() != Nelaric::EInitState::Ready)
				{
					return false;
				}
			}
		}
	}

	TArray<Nelaric::FInitStateSnapshot> BeforeReady;
	BeforeReady.Reserve(Pending.Num());
	for (UActorComponent* Component : Pending)
	{
		const INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Component);
		const Nelaric::FInitStateSnapshot Before{Participant->GetInitGeneration(), Participant->GetInitState(),
		                                         Participant->HasTerminalInitFailure()};
		if (!Participant->CanEnterReady() || GraphVersion != ExpectedGraphVersion ||
		    !(Before.Generation == Participant->GetInitGeneration()) || Before.State != Participant->GetInitState() ||
		    Participant->HasTerminalInitFailure())
		{
			return false;
		}
		BeforeReady.Add(Before);
	}
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		const INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Pending[Index]);
		if (GraphVersion != ExpectedGraphVersion || !RegisteredComponents.Contains(Pending[Index]) ||
		    !(BeforeReady[Index].Generation == Participant->GetInitGeneration()) ||
		    BeforeReady[Index].State != Participant->GetInitState() || Participant->HasTerminalInitFailure())
		{
			return false;
		}
	}
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		INelaricInitStateParticipantInterface* Participant =
		    Cast<INelaricInitStateParticipantInterface>(Pending[Index]);
		if (!ensureMsgf(Participant->CommitReadyWithoutNotification(),
		                TEXT("Ready group member failed its silent commit.")))
		{
			return false;
		}
	}
	for (int32 Index = 0; Index < Pending.Num(); ++Index)
	{
		Cast<INelaricInitStateParticipantInterface>(Pending[Index])->NotifyReadyCommitted(BeforeReady[Index]);
	}
	return true;
}

void UNelaricInitStateWorldSubsystem::NotifyParticipantChanged(UActorComponent* Component,
                                                               const Nelaric::FInitStateSnapshot& Previous)
{
	if (!RegisteredComponents.Contains(Component))
	{
		return;
	}

	const INelaricInitStateParticipantInterface* Participant = Cast<INelaricInitStateParticipantInterface>(Component);
	if (!Participant)
	{
		return;
	}

	const bool bChanged = !(Previous.Generation == Participant->GetInitGeneration()) ||
	                      Previous.State != Participant->GetInitState() ||
	                      Previous.bTerminallyFailed != Participant->HasTerminalInitFailure();
	if (bChanged)
	{
		if (Previous.State == Nelaric::EInitState::Ready &&
		    (Participant->GetInitState() != Nelaric::EInitState::Ready || Participant->HasTerminalInitFailure() ||
		     !(Previous.Generation == Participant->GetInitGeneration())))
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

void UNelaricInitStateWorldSubsystem::RequestParticipantRefresh(UActorComponent* Component)
{
	if (RegisteredComponents.Contains(Component))
	{
		QueueParticipant(Component);
		ProcessParticipants();
	}
}

void UNelaricInitStateWorldSubsystem::ProcessParticipants()
{
	if (bProcessing || bInvalidatingDependents)
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
	while ((!PendingQueue.IsEmpty() || !PendingOwners.IsEmpty()) && Steps < MaxSteps)
	{
		int32 Index = 0;
		while (Index < PendingQueue.Num() && Steps < MaxSteps)
		{
			const FComponentPtr ParticipantPtr = PendingQueue[Index++];
			QueuedComponents.Remove(ParticipantPtr);
			++Steps;
			UActorComponent* Component = ParticipantPtr.Get();
			if (!IsValid(Component) || Component->GetWorld() != GetWorld())
			{
				RegisteredComponents.Remove(ParticipantPtr);
				continue;
			}
			if (!RegisteredComponents.Contains(ParticipantPtr))
			{
				continue;
			}

			INelaricInitStateParticipantInterface* Participant = Cast<INelaricInitStateParticipantInterface>(Component);
			if (!Participant || !Participant->IsInitApplicable() || Participant->HasTerminalInitFailure() ||
			    Participant->GetInitState() == Nelaric::EInitState::Ready)
			{
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
			const bool bExactlyOneStep =
			    Generation == Participant->GetInitGeneration() &&
			    static_cast<uint8>(Participant->GetInitState()) == static_cast<uint8>(PreviousState) + 1;
			if (bAdvanced)
			{
				ensureMsgf(bExactlyOneStep,
				           TEXT("Initialization transition must commit exactly one step in the same generation."));
				if (bExactlyOneStep)
				{
					QueueParticipant(Component);
					if (Participant->GetInitState() == Nelaric::EInitState::Ready)
					{
						QueueParticipantAndDependents(Component);
					}
				}
			}
			else
			{
				ensureMsgf(Generation == Participant->GetInitGeneration() &&
				               PreviousState == Participant->GetInitState(),
				           TEXT("Initialization transition returned false after changing state or generation."));
			}
		}
		PendingQueue.RemoveAt(0, Index, EAllowShrinking::No);
		const TArray<TWeakObjectPtr<AActor>> Owners = PendingOwners.Array();
		PendingOwners.Empty();
		for (const TWeakObjectPtr<AActor>& OwnerPtr : Owners)
		{
			if (AActor* Owner = OwnerPtr.Get())
			{
				TArray<UNelaricPawnInitializationComponent*> Managers;
				Owner->GetComponents<UNelaricPawnInitializationComponent>(Managers);
				for (UNelaricPawnInitializationComponent* Manager : Managers)
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

void UNelaricInitStateWorldSubsystem::Deinitialize()
{
	RegisteredComponents.Empty();
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
