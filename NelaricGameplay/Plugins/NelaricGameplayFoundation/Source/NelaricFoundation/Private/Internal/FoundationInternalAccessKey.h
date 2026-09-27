// Copyright (c) 2026 Nelaric

/** @file FoundationInternalAccessKey.h
 * Declares the passkey used to identify internal C++ integration calls.
 */

#pragma once

namespace Nelaric
{
class FFoundationInternalAccess;

/** @brief Marks C++ methods intended for framework integration.
 *
 * @details Accept this type by const reference in ordinary C++ methods.
 * Only NelaricFoundation implementation files obtain this key.
 * The key signals intended use; it is not an authorization or
 * security boundary.
 */
class FFoundationInternalAccessKey final
{
private:
	FFoundationInternalAccessKey() = default;
	~FFoundationInternalAccessKey() = default;
	FFoundationInternalAccessKey(const FFoundationInternalAccessKey&) = delete;
	FFoundationInternalAccessKey& operator=(const FFoundationInternalAccessKey&) = delete;
	FFoundationInternalAccessKey(FFoundationInternalAccessKey&&) = delete;
	FFoundationInternalAccessKey& operator=(FFoundationInternalAccessKey&&) = delete;

	friend class FFoundationInternalAccess;
};
} // namespace Nelaric
