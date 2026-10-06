// Copyright (c) 2026 Nelaric Contributors

/** @file DemoSquadPlanningSubsystem.h Declares bounded route and lease work. */
#pragma once

#include "AI/DemoSquadTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "DemoSquadPlanningSubsystem.generated.h"

class UDemoSquadContextComponent;

namespace Nelaric::Squad
{
/// Game-thread route result; cancellation delivers one matching terminal call.
DECLARE_DELEGATE_TwoParams(FRouteFinished, EDemoSquadFailure, const TArray<FVector>&);

/// World-owned queued route request; UObject references remain non-owning.
struct FRouteRequest
{
	/// Cancellation identity.
	FGuid Id;
	/// Borrowed member whose navigation agent properties select nav data.
	TWeakObjectPtr<ADemoCharacter> Agent;
	/// Explicit route start, independent of subsequent leader body motion.
	FVector Start = FVector::ZeroVector;
	/// Intended route end.
	FVector End = FVector::ZeroVector;
	/// Absolute timeout in world seconds.
	double Deadline = 0.0;
	/// Terminal callback delivered on the game thread.
	FRouteFinished Finished;
	/// Engine asynchronous request identity; zero before dispatch.
	uint32 NavigationId = 0;
	/// Whether navigation has completed and awaits budgeted delivery.
	bool bResultReady = false;
	/// Terminal navigation outcome, delivered at most twice per frame.
	EDemoSquadFailure ResultFailure = EDemoSquadFailure::None;
	/// Bounded route points retained until budgeted game-thread delivery.
	TArray<FVector> ResultPoints;
};

/// Shared position lease; expired and explicitly released leases are removed.
struct FPositionLease
{
	/// Stable world service lease identity.
	FGuid Id;
	/// World position reserved within the configured separation tolerance.
	FVector Location = FVector::ZeroVector;
	/// Owning member identity.
	FGuid UnitId;
	/// Owning plan identity.
	FGuid PlanId;
	/// Absolute expiry in world seconds.
	double ExpiresAt = 0.0;
};
} // namespace Nelaric::Squad

/** @brief Queues bounded navigation work and arbitrates shared positions.
 * @details World-owned, authority game-thread service. At most two queries
 * are submitted per frame and 128 requests may be outstanding. Teardown
 * cancels engine requests and drops callbacks before destroying the world.
 */
UCLASS(MinimalAPI)
class UDemoSquadPlanningSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	/** @brief Queues one complete route using the agent's navigation settings.
	 * @param Agent Borrowed active friendly character selecting nav data.
	 * @param Start Explicit route start in world centimeters.
	 * @param End Explicit destination in world centimeters.
	 * @param Finished Game-thread terminal callback; may be unbound.
	 * @return Cancellation identity, or invalid when the bounded queue is full.
	 */
	FGuid QueueRoute(ADemoCharacter* Agent, FVector Start, FVector End, Nelaric::Squad::FRouteFinished Finished);
	/// Cancels a matching route and emits Cancelled once; game thread only.
	void CancelRoute(FGuid RequestId);
	/** @brief Projects and reserves a separated position for this plan.
	 * @param UnitId Member identity.
	 * @param PlanId Owning plan identity.
	 * @param Desired Preferred world position in centimeters.
	 * @param[out] Position Validated projection when successful.
	 * @return Lease identity, or invalid when projection or arbitration fails.
	 */
	FGuid ReservePosition(FGuid UnitId, FGuid PlanId, FVector Desired, FVector& Position);
	/// Extends the matching plan lease to ten seconds from now; game thread.
	void RenewPosition(FGuid PositionId, FGuid PlanId);
	/// Releases only the matching plan lease; game thread only.
	void ReleasePosition(FGuid PositionId, FGuid PlanId);

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;
	virtual void Deinitialize() override;

private:
	void FinishRoute(FGuid Id, EDemoSquadFailure Failure, TArray<FVector> Points);
	TArray<Nelaric::Squad::FRouteRequest> Requests;
	TArray<Nelaric::Squad::FPositionLease> Leases;
	bool bStopping = false;
};
