// Copyright (c) 2026 Nelaric Contributors

/** @file FixedSlotPool.h
 * Provides fixed-capacity slot leases without allocating object memory.
 */

#pragma once

#include "HAL/Platform.h"

#include <cstdint>
#include <limits>
#include <type_traits>

namespace Nelaric::ObjectPool
{
/** @brief Allocates slot indices without allocating object memory.
 * @details Uses fixed-width integers, inline arrays, and UE platform macros.
 * @note The owner serializes access and keeps the pool address stable.
 * @note Copy and move are disabled. Handles
 * expire before pool destruction.
 * @tparam Capacity Positive number of slots, excluding two reserved indices.
 */
template <std::uint32_t Capacity> class TFixedSlotPool final
{
public:
	/// Index type with room for the slots and two reserved sentinel values.
	using FIndex = std::conditional_t<(Capacity <= UINT16_MAX - 1u), std::uint16_t, std::uint32_t>;
	/// Sentinel identifying the end of the free list or an empty handle.
	static constexpr FIndex InvalidIndex = std::numeric_limits<FIndex>::max();
	/// Sentinel identifying a slot currently assigned to a lease.
	static constexpr FIndex UsedIndex = InvalidIndex - 1;
	/// Compile-time number of slots managed by this instance.
	static constexpr std::uint32_t CapacityValue = Capacity;

	/** @brief Identifies one logical lease without extending pool lifetime.
	 * @details A token can remain nonempty after Release. Use IsLive to check
	 * its current lease. Never retain it across pool-instance reconstruction.
	 */
	struct FHandle
	{
		/// Non-owning allocator identity; its address must remain stable.
		const TFixedSlotPool* Owner = nullptr;
		/// Lease generation; zero is never issued by the allocator.
		std::uint64_t Generation = 0;
		/// Direct slot index, or InvalidIndex for an empty handle.
		FIndex Index = InvalidIndex;

		/// Returns whether a token was issued, without checking liveness.
		FORCEINLINE explicit operator bool() const noexcept
		{
			return Owner != nullptr;
		}
	};

	/// Initializes every slot as free without allocating memory.
	FORCEINLINE TFixedSlotPool() noexcept
	{
		for (std::uint32_t I = 0; I < Capacity; ++I)
		{
			Next[I] = (I + 1 < Capacity) ? static_cast<FIndex>(I + 1) : InvalidIndex;
		}
	}

	/** @brief Acquires one free slot in constant time.
	 * @return A new lease, or an empty handle when full or generation-saturated.
	 * @details A saturated head stops acquisition without wrapping generation.
	 * Calls must be serialized by the owner; no locks or atomics are used.
	 */
	[[nodiscard]] FHandle TryAcquire() noexcept
	{
		if (Head == InvalidIndex)
		{
			return {};
		}
		const FIndex I = Head;
		if (Generations[I] == UINT64_MAX)
		{
			return {};
		}
		Head = Next[I];
		Next[I] = UsedIndex;
		--FreeCount;
		return {this, ++Generations[I], I};
	}

	/** @brief Checks whether a handle still owns its leased slot.
	 * @param Handle Token to validate against this live allocator instance.
	 * @return True for the current generation of an occupied local slot.
	 */
	[[nodiscard]] FORCEINLINE bool IsLive(FHandle Handle) const noexcept
	{
		return Handle.Owner == this && Handle.Index < Capacity && Next[Handle.Index] == UsedIndex &&
		       Generations[Handle.Index] == Handle.Generation;
	}

	/** @brief Returns a current lease to the free list in constant time.
	 * @param Handle Token for the slot to release.
	 * @return False for empty, foreign, stale, or already released handles.
	 */
	[[nodiscard]] bool Release(FHandle Handle) noexcept
	{
		if (!IsLive(Handle))
		{
			return false;
		}
		Next[Handle.Index] = Head;
		Head = Handle.Index;
		++FreeCount;
		return true;
	}

	/// Returns the number of free slots; owner-serialized access is required.
	FORCEINLINE std::uint32_t NumFree() const noexcept
	{
		return FreeCount;
	}
	/// Returns the number of current leases; access must be serialized.
	FORCEINLINE std::uint32_t NumLeased() const noexcept
	{
		return Capacity - FreeCount;
	}

public:
	TFixedSlotPool(const TFixedSlotPool&) = delete;
	TFixedSlotPool& operator=(const TFixedSlotPool&) = delete;
	TFixedSlotPool(TFixedSlotPool&&) = delete;
	TFixedSlotPool& operator=(TFixedSlotPool&&) = delete;

private:
	static_assert(Capacity > 0, "Capacity must be positive");
	static_assert(Capacity <= UINT32_MAX - 1u, "Two index values are reserved");

	std::uint64_t Generations[Capacity]{};
	FIndex Next[Capacity]{};
	std::uint32_t FreeCount = Capacity;
	FIndex Head = 0;
};
} // namespace Nelaric::ObjectPool
