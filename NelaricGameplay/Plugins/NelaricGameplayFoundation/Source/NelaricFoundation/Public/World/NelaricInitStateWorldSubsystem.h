// Copyright (c) 2026 Nelaric

/** @file NelaricInitStateWorldSubsystem.h
 * Declares the world coordinator for component-owned initialization.
 */

#pragma once

#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "World/NelaricInitStateTypes.h"

#include "NelaricInitStateWorldSubsystem.generated.h"

class UActorComponent;
class AActor;

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
	 * @details Call on the game thread before registering the component. IDs
	 * are resolved only among configured components of the same actor.
	 * @param Component Component owned by the configured actor.
	 * @param ComponentId Stable ID unique on that actor.
	 * @param DependencyIds IDs required to reach Ready.
	 */
	NELARICFOUNDATION_API void ConfigureParticipant(UActorComponent* Component, FName ComponentId,
	                                                const TArray<FName>& DependencyIds);

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
		FName ComponentId;
		TArray<FName> DependencyIds;
	};
	TMap<FComponentPtr, FConfiguredParticipant> ConfiguredComponents;
	bool bProcessing = false;
	bool bProcessRequested = false;
	void ProcessParticipants();
	bool TryCommitReadyGroup(UActorComponent* Root);
	UActorComponent* FindConfiguredComponent(const AActor* Owner, FName ComponentId) const;
	void InvalidateConfiguredDependents(UActorComponent* Component);
};
