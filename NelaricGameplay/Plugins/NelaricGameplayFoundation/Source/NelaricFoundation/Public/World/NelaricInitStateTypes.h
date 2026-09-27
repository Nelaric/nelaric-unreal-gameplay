// Copyright (c) 2026 Nelaric

/** @file NelaricInitStateTypes.h
 * Declares native initialization states and dependency values.
 */

#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtr.h"

class UActorComponent;

namespace Nelaric
{
/// Ordered stages of world initialization.
enum class EInitState : uint8
{
	/// The participant has been registered.
	Registered,

	/// Required data is available.
	DataAvailable,

	/// Required data has been initialized; gameplay cannot run yet.
	DataInitialized,

	/// Initialization is complete and component gameplay may run.
	Ready,
};

/// Identity of one initialization attempt.
struct FInitGeneration
{
	/// Numeric identity of the attempt.
	uint64 Value = 0;

	/// Compares the numeric identities of two attempts.
	bool operator==(const FInitGeneration&) const = default;
};

/// Captures the stage and terminal failure of one initialization attempt.
struct FInitStateSnapshot
{
	/// Attempt to which this snapshot belongs.
	FInitGeneration Generation;

	/// Current initialization stage.
	EInitState State = EInitState::Registered;

	/// Whether the attempt has failed permanently.
	bool bTerminallyFailed = false;
};

/// Identity and current reference of a runtime component dependency.
struct FInitDependency
{
	/// Stable identity supplied even while the component reference is null.
	FName Identity;

	/// Component that must reach Ready; does not keep it alive.
	TWeakObjectPtr<UActorComponent> Component;
};
} // namespace Nelaric
