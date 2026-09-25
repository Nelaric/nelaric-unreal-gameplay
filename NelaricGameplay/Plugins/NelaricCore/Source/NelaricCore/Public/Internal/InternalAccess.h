// Copyright (c) 2026 Nelaric

/** @file InternalAccess.h
 * Provides the shared key for internal C++ integration calls.
 */

#pragma once

#include "Internal/InternalAccessKey.h"

namespace UE::Nelaric
{
/** @brief Supplies the conventional key for framework integration.
 *
 * @details Available to any C++ module depending on NelaricCore.
 * Framework consumers should use supported gameplay APIs. Call an
 * internal method through this key only as a last resort
 * when no suitable public API meets the need.
 */
class NELARICCORE_API FInternalAccess final
{
public:
	/** @brief Returns the shared key for internal C++ calls.
	 *
	 * @details May be called on any thread. The returned immutable
	 * reference remains valid for the process lifetime.
	 *
	 * @return The shared internal access key.
	 */
	static const FInternalAccessKey& Key();
};
} // namespace UE::Nelaric
