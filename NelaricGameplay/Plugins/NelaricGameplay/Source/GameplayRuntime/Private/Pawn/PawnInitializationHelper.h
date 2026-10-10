// Copyright (c) 2026 Nelaric Contributors

/** @file PawnInitializationHelper.h
 * Declares shared access to project pawn initialization components.
 */

#pragma once

#include "CoreMinimal.h"

#include <concepts>

class ANelaricCharacter;
class ANelaricPawn;
class APawn;
class FObjectInitializer;
class UPawnInitializationComponent;

namespace Nelaric::Pawn
{
// Project pawns that store an initialization component directly.
template <typename TPawn>
concept CInitializationPawn = std::derived_from<TPawn, ANelaricPawn> || std::derived_from<TPawn, ANelaricCharacter>;

/** @brief Provides direct access to project pawn initialization components.
 *
 * @details Call on the game thread. The
 * owning pawn retains the component.
 * Returned pointers do not extend its lifetime.
 */
struct FInitializationHelper
{
public:
	/** @brief Reads the initialization component stored by a project pawn.
	 *
	 * @tparam TPawn ANelaricPawn,
	 * ANelaricCharacter, or a derived type.
	 * @param Owner Project pawn to inspect; may be null.
	 * @return
	 * Non-owning component, or null for a null owner.
	 */
	template <CInitializationPawn TPawn> static UPawnInitializationComponent* Get(const TPawn* Owner)
	{
		return Owner ? Owner->PawnInitializationComponent.Get() : nullptr;
	}

	static UPawnInitializationComponent* Create(APawn* Owner, const FObjectInitializer& ObjectInitializer);
};
} // namespace Nelaric::Pawn
