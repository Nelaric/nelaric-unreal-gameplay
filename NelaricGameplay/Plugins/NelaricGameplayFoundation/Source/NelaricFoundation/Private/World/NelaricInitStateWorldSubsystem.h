// Copyright (c) 2026 Nelaric

/** @file NelaricInitStateWorldSubsystem.h
 * Declares the world-owned initialization state subsystem.
 */

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Delegates/Delegate.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "World/NelaricInitStateTypes.h"

#include "NelaricInitStateWorldSubsystem.generated.h"

class UActorComponent;

namespace Nelaric
{
/// Per-component dependency data for the world registry.
struct FNelaricInitDependencyRecord
{
	/// Initialization attempt that produced these dependencies.
	FInitGeneration Generation;

	/// Dependencies gathered for this attempt.
	TArray<FInitDependency> Items;
};

/// Stores local progress and dependency subscriptions for a participant.
struct FNelaricInitHelper
{
private:
	FInitGeneration Generation;
	EInitState State = EInitState::Registered;
	bool bTerminallyFailed = false;
	bool bIsAdvancing = false;
	bool bAdvanceRequested = false;
	TArray<FDelegateHandle> DependencyHandles;
};
} // namespace Nelaric

/** @brief World-scoped host for initialization state coordination.
 *
 * @details Unreal creates one instance per supported world and owns it for
 * that world's lifetime. Initialization state behavior can be added here
 * when its contract is defined. Use on the game thread.
 */
UCLASS(MinimalAPI)
class UNelaricInitStateWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	using FComponentPtr = TWeakObjectPtr<UActorComponent>;

	// Components participating in initialization in this world.
	TSet<FComponentPtr> RegisteredComponents;

	// Dependencies owned by each component.
	TMap<FComponentPtr, Nelaric::FNelaricInitDependencyRecord> Dependencies;

	// Components that depend on each component.
	TMap<FComponentPtr, TSet<FComponentPtr>> Dependents;
};
