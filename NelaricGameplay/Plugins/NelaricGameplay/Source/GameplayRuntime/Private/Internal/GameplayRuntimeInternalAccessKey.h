// Copyright (c) 2026 Nelaric

/** @file GameplayRuntimeInternalAccessKey.h
 * Declares the passkey used to identify internal C++ integration calls.
 */

#pragma once

namespace Nelaric
{
class FGameplayRuntimeInternalAccess;

/** @brief Marks C++ methods intended for framework integration.
 *
 * @details Accept this type by const reference in ordinary C++ methods.
 * Only GameplayRuntime implementation files obtain this key.
 * The key signals intended use; it is not an authorization or
 * security boundary.
 */
class FGameplayRuntimeInternalAccessKey final
{
private:
	FGameplayRuntimeInternalAccessKey() = default;
	~FGameplayRuntimeInternalAccessKey() = default;
	FGameplayRuntimeInternalAccessKey(const FGameplayRuntimeInternalAccessKey&) = delete;
	FGameplayRuntimeInternalAccessKey& operator=(const FGameplayRuntimeInternalAccessKey&) = delete;
	FGameplayRuntimeInternalAccessKey(FGameplayRuntimeInternalAccessKey&&) = delete;
	FGameplayRuntimeInternalAccessKey& operator=(FGameplayRuntimeInternalAccessKey&&) = delete;

	friend class FGameplayRuntimeInternalAccess;
};
} // namespace Nelaric
