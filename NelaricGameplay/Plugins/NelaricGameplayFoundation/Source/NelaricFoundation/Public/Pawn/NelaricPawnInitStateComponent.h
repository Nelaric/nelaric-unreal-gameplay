// Copyright (c) 2026 Nelaric

/** @file NelaricPawnInitStateComponent.h
 * Declares an optional state-owning base for pawn participants.
 */

#pragma once

#include "Pawn/NelaricPawnComponent.h"
#include "World/NelaricInitStateParticipantInterface.h"

#include "NelaricPawnInitStateComponent.generated.h"

/** @brief Optional pawn base implementing the initialization contract.
 *
 * @details Derived classes implement one-step readiness in
 * TryChangeInitState and use CommitInitState for a successful step. They may
 * also implement the interface directly on another UActorComponent. The
 * component owns its state and is called only on the game thread.
 */
UCLASS(Abstract, MinimalAPI)
class UNelaricPawnInitStateComponent : public UNelaricPawnComponent, public INelaricInitStateParticipantInterface
{
	GENERATED_BODY()

public:
	/// Defaults to false until a derived component opts in.
	NELARICFOUNDATION_API virtual bool IsInitApplicable() const override;

	/// Defaults to false until a derived component requires pawn readiness.
	NELARICFOUNDATION_API virtual bool IsRequiredForPawnReady() const override;

	/** @brief Adds no dependencies by default.
	 * @param OutDependencies Array to which dependencies are appended.
	 */
	NELARICFOUNDATION_API virtual void
	GatherInitDependencies(TArray<Nelaric::FInitDependency>& OutDependencies) const override;

	/// Returns false until a derived component commits one adjacent step.
	NELARICFOUNDATION_API virtual bool TryChangeInitState() override;

	/// Returns the state owned by this component.
	NELARICFOUNDATION_API virtual Nelaric::EInitState GetInitState() const override;

	/// Returns the current attempt identity owned by this component.
	NELARICFOUNDATION_API virtual Nelaric::FInitGeneration GetInitGeneration() const override;

	/// Returns whether this attempt has failed permanently.
	NELARICFOUNDATION_API virtual bool HasTerminalInitFailure() const override;

	/** @brief Checks whether an asynchronous result may update this attempt.
	 * @details After dispatching a callback to the game thread, resolve its
	 * weak component reference and call this with the captured world and
	 * generation. Cancellation may not stop work already in flight.
	 * @param ExpectedWorld World captured when work began.
	 * @param ExpectedGeneration Generation captured when work began.
	 * @return Whether the component is still in that live attempt.
	 */
	NELARICFOUNDATION_API bool CanApplyInitResult(const UWorld* ExpectedWorld,
	                                              Nelaric::FInitGeneration ExpectedGeneration) const;

	/// Invalidates old work, resets state, and announces the new attempt.
	NELARICFOUNDATION_API virtual void InvalidateInitGeneration() override;

	/// Records terminal failure and announces it to the world.
	NELARICFOUNDATION_API virtual void MarkTerminalInitFailure() override;

public:
	NELARICFOUNDATION_API UNelaricPawnInitStateComponent(const FObjectInitializer& ObjectInitializer);
	NELARICFOUNDATION_API virtual void OnRegister() override;
	NELARICFOUNDATION_API virtual void OnUnregister() override;

protected:
	/** @brief Commits one adjacent forward step and notifies the world.
	 * @param NextState State immediately after the current state.
	 * @return Whether exactly one step was committed.
	 */
	NELARICFOUNDATION_API bool CommitInitState(Nelaric::EInitState NextState);

	/** @brief Cancels work and removes listeners for an invalidated attempt.
	 * @details Called after the generation changes and before state resets.
	 */
	NELARICFOUNDATION_API virtual void CancelInitGenerationWork();

private:
	Nelaric::EInitState InitState = Nelaric::EInitState::Registered;
	Nelaric::FInitGeneration InitGeneration{1};
	bool bTerminalInitFailure = false;
	bool bCommittingInitState = false;
	void NotifyInitChanged(const Nelaric::FInitStateSnapshot& Previous);
};
