// Copyright (c) 2026 Nelaric

/** @file NelaricInitStateWorldSubsystem.h
 * Declares the world coordinator for component-owned initialization.
 */

#pragma once

#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "World/NelaricInitStateTypes.h"

#include "NelaricInitStateWorldSubsystem.generated.h"

class UActorComponent;
class AActor;

namespace Nelaric
{
/** @brief One resolved, local participant and its Ready dependencies.
 * @details The pawn initialization component supplies these entries before
 * registering its components. References do not transfer ownership.
 */
struct FInitParticipantConfiguration
{
	/// Component owned by the local actor.
	UActorComponent* Component = nullptr;

	/// Stable ID unique on the actor.
	FName ComponentId;

	/// Whether pawn readiness includes this component.
	bool bRequiredForPawnReady = true;

	/// Local Ready dependencies; cyclic peers commit together.
	TArray<UActorComponent*> Dependencies;
};
} // namespace Nelaric

/** @brief Coordinates registered initialization participants in one world.
 *
 * @details Unreal owns the subsystem for the world lifetime. Components own
 * all state; this service keeps only weak registrations. Call on the game
 * thread. A participant must register and unregister with its component.
 */
UCLASS(MinimalAPI)
class UNelaricInitStateWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** @brief Registers a reflected participant and attempts progress.
	 * @param Component Component in this world; does not transfer ownership.
	 */
	NELARICFOUNDATION_API void RegisterParticipant(UActorComponent* Component);

	/** @brief Removes a participant before it leaves the world.
	 * @param Component Component that was previously registered.
	 */
	NELARICFOUNDATION_API void UnregisterParticipant(UActorComponent* Component);

	/** @brief Reports a component-owned state or generation change.
	 * @param Component Changed registered component.
	 * @param Previous Snapshot captured before the component changed.
	 */
	NELARICFOUNDATION_API void NotifyParticipantChanged(UActorComponent* Component,
	                                                    const Nelaric::FInitStateSnapshot& Previous);

	/** @brief Rechecks a registered participant after its context changes.
	 * @details Call when a declared dependency reference becomes available.
	 * @param Component Registered component requesting another progress pass.
	 */
	NELARICFOUNDATION_API void RequestParticipantRefresh(UActorComponent* Component);

	/** @brief Installs the manager's immutable configuration for a component.
	 * @details Call on the game thread before registering any configured
	 * component. Dependency references do not transfer ownership.
	 * @param Component Component owned by the configured actor.
	 * @param ComponentId Stable ID unique on that actor.
	 * @param bRequiredForPawnReady Whether pawn readiness includes this member.
	 * @param Dependencies Components required to reach Ready.
	 */
	NELARICFOUNDATION_API void ConfigureParticipant(UActorComponent* Component, FName ComponentId,
	                                                bool bRequiredForPawnReady,
	                                                const TArray<UActorComponent*>& Dependencies);

	/** @brief Installs a resolved local dependency graph in one update.
	 * @details Call on the game thread before registering its components.
	 * Cycles form Ready commit groups; other dependencies order Ready.
	 * @param Configurations Entries resolved against local actor components.
	 */
	NELARICFOUNDATION_API void
	ConfigureParticipants(const TArray<Nelaric::FInitParticipantConfiguration>& Configurations);

	/** @brief Checks the configured required participants of an actor.
	 * @details Call on the game thread after configuration. Missing or failed
	 * required participants prevent readiness.
	 * @param Owner Actor whose required participants are checked.
	 * @return Whether all required participants have reached Ready.
	 */
	NELARICFOUNDATION_API bool AreRequiredParticipantsReady(const AActor* Owner) const;

	/** @brief Checks a configured participant's current Ready state.
	 * @param Component Component supplied by its owning initialization manager.
	 * @return Whether the participant is registered, applicable, and Ready.
	 */
	NELARICFOUNDATION_API bool IsParticipantReady(UActorComponent* Component) const;

	/** @brief Removes the manager's configuration for a component.
	 * @param Component Component leaving its configured pawn.
	 */
	NELARICFOUNDATION_API void UnconfigureParticipant(UActorComponent* Component);

public:
	virtual void Deinitialize() override;

private:
	using FComponentPtr = TWeakObjectPtr<UActorComponent>;
	TSet<FComponentPtr> RegisteredComponents;
	struct FConfiguredParticipant
	{
		TWeakObjectPtr<AActor> Owner;
		FName ComponentId;
		bool bRequiredForPawnReady = true;
		TArray<FComponentPtr> Dependencies;
	};
	TMap<FComponentPtr, FConfiguredParticipant> ConfiguredComponents;
	TMap<FComponentPtr, TArray<FComponentPtr>> ForwardDependencies;
	TMap<FComponentPtr, TArray<FComponentPtr>> ReverseDependencies;
	TMap<TWeakObjectPtr<AActor>, TArray<FComponentPtr>> RequiredByOwner;
	TMap<FComponentPtr, int32> ReadyGroupByComponent;
	TArray<TArray<FComponentPtr>> ReadyGroups;
	uint64 GraphVersion = 0;
	int32 GraphEdgeCount = 0;
	TArray<FComponentPtr> PendingQueue;
	TSet<FComponentPtr> QueuedComponents;
	TSet<TWeakObjectPtr<AActor>> PendingOwners;
	bool bProcessing = false;
	bool bInvalidatingDependents = false;
	void RebuildDependencyGraph();
	void QueueParticipant(UActorComponent* Component);
	void QueueParticipantAndDependents(UActorComponent* Component);
	void QueueAllRegistered();
	void ProcessParticipants();
	bool TryCommitReadyGroup(UActorComponent* Root);
	void InvalidateConfiguredDependents(UActorComponent* Component);
};
