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
 * @details Use CanEntry for preparation and OnInitReady for gameplay.
 * The first three return true when their
 * work is complete.
 * Pending stages retry after RequestInitRefresh. Calls use the game thread.
 */
UCLASS(Abstract, MinimalAPI)
class UNelaricPawnInitStateComponent : public UNelaricPawnComponent, public INelaricInitStateParticipantInterface
{
	GENERATED_BODY()

public:
	/// Participates by default; override for conditional applicability.
	NELARICFOUNDATION_API virtual bool IsInitApplicable() const override;

	/// Commits at most one adjacent preparation step when the derived gate passes.
	NELARICFOUNDATION_API virtual bool TryChangeInitState() final override;

	/// Runs DataInitialized preparation until it succeeds in this generation.
	NELARICFOUNDATION_API virtual bool CanEnterReady() final override;

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
	/** @brief Acquires this component's data while Registered.
	 * @details Retry-safe work may run again after a
	 * refresh. Defaults to true.
	 * @return True to enter DataAvailable; false to keep waiting.
	 */
	NELARICFOUNDATION_API virtual bool CanEntryDataAvailable();

	/** @brief Initializes this component's available data.
	 * @details Retry-safe work may run again after a
	 * refresh. Defaults to true.
	 * @return True to enter DataInitialized; false to keep waiting.
	 */
	NELARICFOUNDATION_API virtual bool CanEntryDataInitialized();

	/** @brief Performs final local preparation before the Ready check.
	 * @details Retry-safe work may run again
	 * until true, then remains complete
	 * for this generation. Defaults to true. Gameplay requires Ready.
	 *
	 * @return Whether local preparation for Ready is complete.
	 */
	NELARICFOUNDATION_API virtual bool CanEntryReady();

	/** @brief Starts gameplay after the entire readiness group entered Ready.
	 * @details Called once per Ready entry,
	 * after every group member commits.
	 */
	NELARICFOUNDATION_API virtual void OnInitReady();

	/** @brief Stops work and gameplay belonging to an invalidated attempt.
	 * @details The generation has changed.
	 * The old state remains readable.
	 * Preparation resumes through the coordinator after the reset.
	 *
	 * @param Previous Snapshot before invalidation.
	 */
	NELARICFOUNDATION_API virtual void OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot& Previous);

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
	bool bLocalReadyPrepared = false;
	bool bLeavingWorld = false;
	bool bRefreshRequestedDuringTransition = false;
	void NotifyInitChanged(const Nelaric::FInitStateSnapshot& Previous);
	void FlushDeferredInitRefresh();
};
