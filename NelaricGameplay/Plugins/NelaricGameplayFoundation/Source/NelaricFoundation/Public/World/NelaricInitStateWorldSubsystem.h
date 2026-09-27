// Copyright (c) 2026 Nelaric

/** @file NelaricInitStateWorldSubsystem.h
 * Declares the world coordinator for component-owned initialization.
 */

#pragma once

#include "Containers/Set.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "World/NelaricInitStateTypes.h"

#include "NelaricInitStateWorldSubsystem.generated.h"

class UActorComponent;

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

public:
	virtual void Deinitialize() override;

private:
	using FComponentPtr = TWeakObjectPtr<UActorComponent>;
	TSet<FComponentPtr> RegisteredComponents;
	bool bProcessing = false;
	bool bProcessRequested = false;
	void ProcessParticipants();
};
