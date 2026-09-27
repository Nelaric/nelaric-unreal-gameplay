// Copyright (c) 2026 Nelaric

/** @file NelaricPawnInitStateComponent.h
 * Declares the base for native pawn initialization participants.
 */

#pragma once

#include "Pawn/NelaricPawnComponent.h"
#include "World/NelaricInitStateParticipantInterface.h"

#include "NelaricPawnInitStateComponent.generated.h"

/** @brief Base for pawn components using the native init-state contract.
 *
 * @details Foundation derives from this internal base to implement pawn
 * initialization behavior. The pawn owns the component. Call on the game
 * thread. Defaults do not advance state or add dependencies.
 */
UCLASS(Abstract, MinimalAPI)
class UNelaricPawnInitStateComponent : public UNelaricPawnComponent, public Nelaric::INelaricInitStateParticipantInterface
{
	GENERATED_BODY()

public:
	/// Returns false until a derived component opts into initialization.
	virtual bool IsInitApplicable() const override;

	/// Returns false until a derived component requires pawn readiness.
	virtual bool IsRequiredForPawnReady() const override;

	/** @brief Adds no dependencies unless overridden by a derived component.
	 * @param OutDependencies Array to which dependencies may be appended.
	 */
	virtual void GatherInitDependencies(TArray<Nelaric::FInitDependency>& OutDependencies) const override;

	/// Returns false until a derived component can advance its state.
	virtual bool TryChangeInitState() override;

public:
	UNelaricPawnInitStateComponent(const FObjectInitializer& ObjectInitializer);
};
