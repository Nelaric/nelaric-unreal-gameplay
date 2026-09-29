// Copyright (c) 2026 Nelaric

/** @file GameplayRuntimeInternalAccess.h
 * Provides the shared key for internal C++ integration calls.
 */

#pragma once

#include "Internal/GameplayRuntimeInternalAccessKey.h"

namespace Nelaric
{
/** @brief Supplies the conventional key for framework integration.
 *
 * @details Available only inside the GameplayRuntime module.
 * Gameplay callers use supported public APIs.
 */
class FGameplayRuntimeInternalAccess final
{
public:
	/** @brief Returns the shared key for internal C++ calls.
	 *
	 * @details May be called on any thread. The returned immutable
	 * reference remains valid for the process lifetime.
	 *
	 * @return The shared internal access key.
	 */
	static const FGameplayRuntimeInternalAccessKey& Key();
};
} // namespace Nelaric
