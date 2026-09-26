// Copyright (c) 2026 Nelaric

/** @file InternalAccessKey.h
 * Declares the passkey used to identify internal C++ integration calls.
 */

#pragma once

namespace Nelaric
{
class FInternalAccess;

/** @brief Marks C++ methods intended for framework integration.
 *
 * @details Accept this type by const reference in ordinary C++ methods.
 * Any module depending on NelaricCore may obtain the key through
 * FInternalAccess. The key signals intended use; it is not an
 * authorization or security boundary.
 */
class FInternalAccessKey final
{
private:
	FInternalAccessKey() = default;
	~FInternalAccessKey() = default;
	FInternalAccessKey(const FInternalAccessKey&) = delete;
	FInternalAccessKey& operator=(const FInternalAccessKey&) = delete;
	FInternalAccessKey(FInternalAccessKey&&) = delete;
	FInternalAccessKey& operator=(FInternalAccessKey&&) = delete;

	friend class FInternalAccess;
};
} // namespace Nelaric
