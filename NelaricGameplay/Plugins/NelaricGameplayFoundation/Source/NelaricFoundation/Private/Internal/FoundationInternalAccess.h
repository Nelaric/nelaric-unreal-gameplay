// Copyright (c) 2026 Nelaric

/** @file FoundationInternalAccess.h
 * Provides the shared key for internal C++ integration calls.
 */

#pragma once

#include "Internal/FoundationInternalAccessKey.h"

namespace Nelaric
{
/** @brief Supplies the conventional key for framework integration.
 *
 * @details Available only inside the NelaricFoundation module.
 * Gameplay callers use supported public APIs.
 */
class FFoundationInternalAccess final
{
public:
	/** @brief Returns the shared key for internal C++ calls.
	 *
	 * @details May be called on any thread. The returned immutable
	 * reference remains valid for the process lifetime.
	 *
	 * @return The shared internal access key.
	 */
	static const FFoundationInternalAccessKey& Key();
};
} // namespace Nelaric
