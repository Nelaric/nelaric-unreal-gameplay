// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSquadPlanningSubsystem.h"
#include "Character/DemoCharacter.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "NavigationPath.h"

FGuid UDemoSquadPlanningSubsystem::QueueRoute(ADemoCharacter* Agent, FVector Start, FVector End,
                                              Nelaric::Squad::FRouteFinished Finished)
{
	check(IsInGameThread());
	if (bStopping || Requests.Num() >= 128 || !IsValid(Agent) || Agent->GetWorld() != GetWorld() ||
	    !Agent->HasAuthority() || Start.ContainsNaN() || End.ContainsNaN())
	{
		return {};
	}
	Nelaric::Squad::FRouteRequest Request;
	Request.Id = FGuid::NewGuid();
	Request.Agent = Agent;
	Request.Start = Start;
	Request.End = End;
	Request.Deadline = GetWorld()->GetTimeSeconds() + 5.0;
	Request.Finished = MoveTemp(Finished);
	const FGuid Id = Request.Id;
	Requests.Add(MoveTemp(Request));
	return Id;
}

void UDemoSquadPlanningSubsystem::FinishRoute(FGuid Id, EDemoSquadFailure Failure, TArray<FVector> Points)
{
	const int32 Index = Requests.IndexOfByPredicate([Id](const auto& Request) { return Request.Id == Id; });
	if (Index == INDEX_NONE)
	{
		return;
	}
	auto Callback = MoveTemp(Requests[Index].Finished);
	Requests.RemoveAt(Index);
	Callback.ExecuteIfBound(Failure, Points);
}

void UDemoSquadPlanningSubsystem::CancelRoute(FGuid RequestId)
{
	check(IsInGameThread());
	const auto* Request = Requests.FindByPredicate([RequestId](const auto& Entry) { return Entry.Id == RequestId; });
	if (Request && Request->NavigationId != 0)
	{
		if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			Nav->AbortAsyncFindPathRequest(Request->NavigationId);
		}
	}
	FinishRoute(RequestId, EDemoSquadFailure::Cancelled, {});
}

FGuid UDemoSquadPlanningSubsystem::ReservePosition(FGuid UnitId, FGuid PlanId, FVector Desired, FVector& Position)
{
	check(IsInGameThread());
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (bStopping || !UnitId.IsValid() || !PlanId.IsValid() || !Nav || Desired.ContainsNaN() ||
	    !Nav->ProjectPointToNavigation(Desired, Projected, FVector(150.0, 150.0, 300.0)))
	{
		return {};
	}
	const double Now = GetWorld()->GetTimeSeconds();
	Leases.RemoveAll([Now](const auto& Lease) { return Lease.ExpiresAt <= Now; });
	for (const auto& Lease : Leases)
	{
		if ((Lease.UnitId != UnitId || Lease.PlanId != PlanId) &&
		    FVector::DistSquared2D(Lease.Location, Projected.Location) < FMath::Square(120.0f))
		{
			return {};
		}
	}
	if (Leases.Num() >= 4096)
	{
		return {};
	}
	Nelaric::Squad::FPositionLease Lease;
	Lease.Id = FGuid::NewGuid();
	Lease.UnitId = UnitId;
	Lease.PlanId = PlanId;
	Lease.Location = Projected.Location;
	Lease.ExpiresAt = Now + 10.0;
	Leases.Add(Lease);
	Position = Projected.Location;
	return Lease.Id;
}

void UDemoSquadPlanningSubsystem::RenewPosition(FGuid PositionId, FGuid PlanId)
{
	if (auto* Lease = Leases.FindByPredicate([PositionId, PlanId](const auto& Entry)
	                                         { return Entry.Id == PositionId && Entry.PlanId == PlanId; }))
	{
		Lease->ExpiresAt = GetWorld()->GetTimeSeconds() + 10.0;
	}
}

void UDemoSquadPlanningSubsystem::ReleasePosition(FGuid PositionId, FGuid PlanId)
{
	Leases.RemoveAll([PositionId, PlanId](const auto& Lease)
	                 { return Lease.Id == PositionId && Lease.PlanId == PlanId; });
}

void UDemoSquadPlanningSubsystem::Tick(float DeltaTime)
{
	check(IsInGameThread());
	const double Now = GetWorld()->GetTimeSeconds();
	TArray<FGuid> Expired;
	for (const auto& Request : Requests)
	{
		if (!Request.bResultReady && Request.Deadline <= Now)
		{
			Expired.Add(Request.Id);
		}
	}
	for (FGuid Id : Expired)
	{
		// Remove the callback first so cancellation cannot report twice.
		const auto* Request = Requests.FindByPredicate([Id](const auto& Entry) { return Entry.Id == Id; });
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (Request && Nav && Request->NavigationId != 0)
		{
			Nav->AbortAsyncFindPathRequest(Request->NavigationId);
		}
		FinishRoute(Id, EDemoSquadFailure::PhaseTimeout, {});
	}
	TArray<FGuid> Ready;
	for (const auto& Request : Requests)
	{
		if (Request.bResultReady && Ready.Num() < 2)
		{
			Ready.Add(Request.Id);
		}
	}
	for (FGuid Id : Ready)
	{
		auto* Request = Requests.FindByPredicate([Id](const auto& Entry) { return Entry.Id == Id; });
		if (Request)
		{
			const auto Failure = Request->ResultFailure;
			auto Points = MoveTemp(Request->ResultPoints);
			FinishRoute(Id, Failure, MoveTemp(Points));
		}
	}
	TArray<FGuid> Submit;
	for (const auto& Request : Requests)
	{
		if (!Request.bResultReady && Request.NavigationId == 0 && Submit.Num() < 2)
		{
			Submit.Add(Request.Id);
		}
	}
	for (FGuid Id : Submit)
	{
		auto* Request = Requests.FindByPredicate([Id](const auto& Entry) { return Entry.Id == Id; });
		ADemoCharacter* Agent = Request ? Request->Agent.Get() : nullptr;
		UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		const ANavigationData* Data =
		    Nav && Agent ? Nav->GetNavDataForProps(Agent->GetNavAgentPropertiesRef(), Request->Start) : nullptr;
		FNavLocation End;
		if (!Request || !Agent || !Agent->IsAlive() || !Agent->IsPoolActive() || !Data ||
		    !Nav->ProjectPointToNavigation(Request->End, End, FVector(150.0, 150.0, 300.0), Data))
		{
			FinishRoute(Id, EDemoSquadFailure::Unreachable, {});
			continue;
		}
		FPathFindingQuery Query(Agent, *Data, Request->Start, End.Location);
		Query.SetAllowPartialPaths(false);
		FNavPathQueryDelegate Callback;
		Callback.BindWeakLambda(
		    this,
		    [this, Id](uint32, ENavigationQueryResult::Type Result, FNavPathSharedPtr Path)
		    {
			    TArray<FVector> Points;
			    if (!bStopping && Result == ENavigationQueryResult::Success && Path.IsValid() && !Path->IsPartial())
			    {
				    for (const FNavPathPoint& Point : Path->GetPathPoints())
				    {
					    if (Points.Num() >= 256)
					    {
						    FinishRoute(Id, EDemoSquadFailure::Unreachable, {});
						    return;
					    }
					    Points.Add(Point.Location);
				    }
			    }
			    const EDemoSquadFailure Failure =
			        Points.IsEmpty() ? EDemoSquadFailure::Unreachable : EDemoSquadFailure::None;
			    if (auto* Live = Requests.FindByPredicate([Id](const auto& Entry) { return Entry.Id == Id; }))
			    {
				    Live->bResultReady = true;
				    Live->ResultFailure =
				        GetWorld()->GetTimeSeconds() > Live->Deadline ? EDemoSquadFailure::PhaseTimeout : Failure;
				    Live->ResultPoints = MoveTemp(Points);
			    }
		    });
		const uint32 NavigationId = Nav->FindPathAsync(Agent->GetNavAgentPropertiesRef(), Query, Callback);
		if (auto* Live = Requests.FindByPredicate([Id](const auto& Entry) { return Entry.Id == Id; }))
		{
			Live->NavigationId = NavigationId;
			if (NavigationId == 0)
			{
				FinishRoute(Id, EDemoSquadFailure::Unreachable, {});
			}
		}
	}
}

TStatId UDemoSquadPlanningSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDemoSquadPlanningSubsystem, STATGROUP_Tickables);
}

bool UDemoSquadPlanningSubsystem::IsTickable() const
{
	return !bStopping && !IsTemplate() && !Requests.IsEmpty();
}

void UDemoSquadPlanningSubsystem::Deinitialize()
{
	bStopping = true;
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		for (const auto& Request : Requests)
		{
			if (Request.NavigationId != 0)
			{
				Nav->AbortAsyncFindPathRequest(Request.NavigationId);
			}
		}
	}
	Requests.Reset();
	Leases.Reset();
	Super::Deinitialize();
}
