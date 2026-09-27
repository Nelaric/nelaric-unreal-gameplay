// Copyright (c) 2026 Nelaric

/** @file NelaricPawnInitStateComponent.h
 * Declares an optional state-owning base for pawn participants.
 */

#pragma once

#include "Pawn/NelaricPawnComponent.h"
#include "Pawn/NelaricInitStateParticipantInterface.h"

#include "NelaricPawnInitStateComponent.generated.h"

/** @brief Optional pawn base implementing the initialization contract.
 *
 * @details Derived classes implement preparation through DataInitialized in
 * TryChangeInitState and a pure CanEnterReady check. They may
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

	/// Commits at most one adjacent preparation step when the derived gate passes.
	NELARICFOUNDATION_API virtual bool TryChangeInitState() final override;

	/// Returns false until internal preparation is ready.
	NELARICFOUNDATION_API virtual bool CanEnterReady() const override;

	/// Writes Ready without callbacks after the coordinator checks the group.
	NELARICFOUNDATION_API virtual bool CommitReadyWithoutNotification() override;

	/** @brief Announces a committed Ready transition after the group commits.
	 * @param Previous Snapshot captured before the Ready commit.
	 */
	NELARICFOUNDATION_API virtual void NotifyReadyCommitted(const Nelaric::FInitStateSnapshot& Previous) override;

	/** @brief Requests another local coordinator pass after context changes.
	 * @details Call from a replicated gameplay-data arrival event, such as
	 * OnRep, after updating local data used by readiness checks.
	 */
	NELARICFOUNDATION_API void RequestInitRefresh();

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

	/** @brief Resolves a game-thread asynchronous completion for a live attempt.
	 * @param WeakComponent Component captured weakly when work began.
	 * @param ExpectedWorld World captured when work began.
	 * @param ExpectedGeneration Generation captured when work began.
	 * @return Live component, or null after removal, travel, or invalidation.
	 */
	NELARICFOUNDATION_API static UNelaricPawnInitStateComponent*
	ResolveInitResult(const TWeakObjectPtr<UNelaricPawnInitStateComponent>& WeakComponent, const UWorld* ExpectedWorld,
	                  Nelaric::FInitGeneration ExpectedGeneration);

	/// Invalidates old work, resets state, and announces the new attempt.
	NELARICFOUNDATION_API virtual void InvalidateInitGeneration() override;

	/// Records terminal failure and announces it to the world.
	NELARICFOUNDATION_API virtual void MarkTerminalInitFailure() override;

public:
	NELARICFOUNDATION_API UNelaricPawnInitStateComponent(const FObjectInitializer& ObjectInitializer);
	NELARICFOUNDATION_API virtual void OnRegister() override;
	NELARICFOUNDATION_API virtual void OnUnregister() override;

protected:
	/** @brief Starts this registered attempt's own data acquisition.
	 * @details Called on the game thread after
	 * entering Registered and before
	 * the coordinator can advance this attempt. Gameplay must remain inactive.

	 */
	NELARICFOUNDATION_API virtual void OnInitRegistered();

	/** @brief Starts initialization using data now available to this component.
	 * @details Called once on entering
	 * DataAvailable in each generation.
	 */
	NELARICFOUNDATION_API virtual void OnInitDataAvailable();

	/** @brief Performs final local preparation before the Ready check.
	 * @details Called once on entering
	 * DataInitialized in each generation.
	 * Runtime dependency gameplay must remain inactive.
	 */
	NELARICFOUNDATION_API virtual void OnInitDataInitialized();

	/** @brief Starts gameplay after the entire readiness group entered Ready.
	 * @details Called once per Ready entry,
	 * after every group member commits.
	 */
	NELARICFOUNDATION_API virtual void OnInitReady();

	/** @brief Stops work and gameplay belonging to an invalidated attempt.
	 * @details The generation has changed.
	 * The old state remains readable.
	 * OnInitRegistered follows the reset when the component is registered.
	 *
	 * @param Previous Snapshot before invalidation.
	 */
	NELARICFOUNDATION_API virtual void OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot& Previous);

	/** @brief Checks whether the next preparation step can be committed.
	 * @details Derived implementations may
	 * start asynchronous work here.
	 * Request a refresh after it completes. Do not change state here.
	 * @return
	 * Whether one adjacent preparation step can be committed now.
	 */
	NELARICFOUNDATION_API virtual bool CanAdvanceInitState();

	/** @brief Cancels work and removes listeners for an invalidated attempt.
	 * @details Called after the generation changes and before state resets.
	 */
	NELARICFOUNDATION_API virtual void CancelInitGenerationWork();

private:
	bool CommitInitState(Nelaric::EInitState NextState);
	Nelaric::EInitState InitState = Nelaric::EInitState::Registered;
	Nelaric::FInitGeneration InitGeneration{1};
	bool bTerminalInitFailure = false;
	bool bCommittingInitState = false;
	bool bReadyNotificationPending = false;
	bool bLeavingWorld = false;
	bool bRefreshRequestedDuringTransition = false;
	void NotifyInitChanged(const Nelaric::FInitStateSnapshot& Previous);
	void FlushDeferredInitRefresh();
};
