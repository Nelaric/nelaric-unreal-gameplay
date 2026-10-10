// Copyright (c) 2026 Nelaric Contributors

/** @file PoolNetwork.h
 * Declares compile-time pool networking and automatic client views.
 */

#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

namespace Nelaric::ObjectPool
{
/// Selects whether the pool publishes membership through UE connections.
enum class ENetworkMode : uint8
{
	/// Keeps all pool operations local to their owner.
	Disabled,
	/// Publishes authority slots and creates non-owning client views.
	Replicated,
};

/** @brief Accepts only the supported pool networking modes.
 * @tparam Mode Local operation or automatic replication.
 */
template <ENetworkMode Mode>
concept CPoolNetworkMode = Mode == ENetworkMode::Disabled || Mode == ENetworkMode::Replicated;

namespace Private
{
struct FPoolNetworkStorage;

/** @brief Binds a native pool to the framework's shared network channel.
 * @details Used by the replicated template specialization on the game thread.
 * Client views never create actors or grant authority leases.
 */
class FPoolNetworkBinding
{
public:
public:
	GAMEPLAYRUNTIME_API bool Begin(UWorld& World, FName Name, uint32 Capacity);
	GAMEPLAYRUNTIME_API void Publish(uint32 Index, AActor* Actor, bool bActive);
	GAMEPLAYRUNTIME_API void Close();
	GAMEPLAYRUNTIME_API bool IsClient() const;
	GAMEPLAYRUNTIME_API bool IsReady() const;
	GAMEPLAYRUNTIME_API AActor* Get(uint32 Index) const;

private:
	TSharedPtr<FPoolNetworkStorage> Storage;
};

/// Empty specialization keeps local pools free of network state.
template <ENetworkMode Mode>
    requires CPoolNetworkMode<Mode>
struct TPoolNetworkBinding
{
};

/// Replicated pools retain one automatically managed network binding.
template <> struct TPoolNetworkBinding<ENetworkMode::Replicated> : FPoolNetworkBinding
{
};
} // namespace Private

/** @brief Reads framework-applied replica state on the game thread.
 * @param Actor Local actor whose replica membership is queried.
 * @param[out] bActive Applied activity state when membership exists.
 * @return Whether the actor belongs to an automatically received pool.
 */
GAMEPLAYRUNTIME_API bool QueryPoolReplica(const AActor& Actor, bool& bActive);
} // namespace Nelaric::ObjectPool
