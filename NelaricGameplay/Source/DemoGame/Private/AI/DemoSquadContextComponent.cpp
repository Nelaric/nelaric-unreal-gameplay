// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoSquadContextComponent.h"
#include "AI/DemoSquadDefinition.h"
#include "AI/DemoSquadCommandActor.h"
#include "AI/DemoSquadMemberComponent.h"
#include "AI/DemoSquadOrderReceiverComponent.h"
#include "AI/DemoSquadPlanningSubsystem.h"
#include "Character/DemoCharacter.h"
#include "Components/StateTreeComponent.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "EngineUtils.h"
#include "NativeGameplayTags.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace Nelaric::Squad
{
UE_DEFINE_GAMEPLAY_TAG_STATIC(DecisionChanged, "AI.Squad.DecisionChanged");

static bool ValidConfiguration(const UDemoSquadDefinition& Definition, const UDemoSquadTactics& Tactics)
{
	const float Values[] = {Definition.RetainedOrderSeconds, Tactics.FeedbackTimeoutSeconds, Tactics.SlotSpacing,
	                        Tactics.ContactDecay, Tactics.ContactUncertaintyGrowth};
	for (float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f)
		{
			return false;
		}
	}
	return Definition.MemberLimit > 0 && Definition.MemberLimit <= 64 && Definition.SupportGroupSize > 0 &&
	       Definition.SupportGroupSize <= 32 && Definition.MinimumReadySupport > 0 &&
	       Definition.MinimumReadySupport <= Definition.SupportGroupSize && Definition.RetainedOrderSeconds > 0.0f &&
	       Tactics.FeedbackTimeoutSeconds > 0.0f && Tactics.SlotSpacing >= 100.0f && Tactics.ContactDecay > 0.0f;
}

static bool ValidMissionArea(const FDemoSquadArea& Area)
{
	return !Area.Center.ContainsNaN() && FMath::IsFinite(Area.Radius) && Area.Radius > 0.0f;
}

static bool Inside(const FDemoSquadArea& Area, FVector Position)
{
	return FVector::DistSquared2D(Area.Center, Position) <= FMath::Square(Area.Radius);
}

static UDemoSquadOrderReceiverComponent* Receiver(const FDemoSquadMemberStatus* Member)
{
	ADemoCharacter* Character = Member ? Member->Character.Get() : nullptr;
	return Character ? Character->FindComponentByClass<UDemoSquadOrderReceiverComponent>() : nullptr;
}

} // namespace Nelaric::Squad

UDemoSquadContextComponent::UDemoSquadContextComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDemoSquadContextComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!SquadId.IsValid())
		SquadId = FGuid::NewGuid();
}

const UDemoSquadDefinition& UDemoSquadContextComponent::GetDefinition() const
{
	return Definition ? *Definition : *GetDefault<UDemoSquadDefinition>();
}

const UDemoSquadTactics& UDemoSquadContextComponent::GetTactics() const
{
	return Tactics ? *Tactics : *GetDefault<UDemoSquadTactics>();
}

FDemoSquadMemberStatus* UDemoSquadContextComponent::FindMember(FGuid UnitId)
{
	return Members.FindByPredicate([UnitId](const auto& Member) { return Member.UnitId == UnitId; });
}

const FDemoSquadMemberStatus* UDemoSquadContextComponent::FindMember(FGuid UnitId) const
{
	return Members.FindByPredicate([UnitId](const auto& Member) { return Member.UnitId == UnitId; });
}

bool UDemoSquadContextComponent::RegisterMember(UDemoSquadMemberComponent* Member)
{
	check(IsInGameThread());
	if (bEnding || !GetOwner()->HasAuthority() || !IsValid(Member) || Member->GetWorld() != GetWorld())
	{
		return false;
	}
	FDemoSquadMemberStatus Status = Member->CaptureStatus();
	ADemoCharacter* Character = Status.Character.Get();
	if (!Character || !Status.UnitId.IsValid() || FindMember(Status.UnitId) ||
	    Members.Num() >= FMath::Clamp(GetDefinition().MemberLimit, 1, 64))
	{
		return false;
	}
	for (const auto& Existing : Members)
	{
		if (Existing.Character == Character ||
		    (Existing.Character.IsValid() && Existing.Character->GetTeamId() != Character->GetTeamId()))
		{
			return false;
		}
	}
	Members.Add(Status);
	MemberComponents.Add(Status.UnitId, Member);
	Wake();
	return true;
}

void UDemoSquadContextComponent::UnregisterMember(FGuid UnitId, int32 BindingGeneration)
{
	check(IsInGameThread());
	const FDemoSquadMemberStatus* Member = FindMember(UnitId);
	if (!Member || Member->BindingGeneration != BindingGeneration)
	{
		return;
	}
	for (const auto& Assignment : Plan.Assignments)
	{
		if (Assignment.UnitId == UnitId)
		{
			if (UDemoSquadPlanningSubsystem* Service = GetWorld()->GetSubsystem<UDemoSquadPlanningSubsystem>())
			{
				Service->ReleasePosition(Assignment.PositionId, Plan.Identity.PlanId);
			}
		}
	}
	Members.RemoveAll([UnitId](const auto& Entry) { return Entry.UnitId == UnitId; });
	MemberComponents.Remove(UnitId);
	Wake();
}

void UDemoSquadContextComponent::ReportMember(const FDemoSquadMemberStatus& Status)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Squad_Feedback);
	check(IsInGameThread());
	FDemoSquadMemberStatus* Existing = FindMember(Status.UnitId);
	if (bEnding || !GetOwner()->HasAuthority() || !Existing ||
	    Existing->BindingGeneration != Status.BindingGeneration || Existing->Character != Status.Character ||
	    Status.ReportedAt < Existing->ReportedAt)
	{
		return;
	}
	if (Status.TeamId != Existing->TeamId)
	{
		if (const auto* Entry = MemberComponents.Find(Status.UnitId))
		{
			if (auto* Member = Entry->Get())
			{
				Member->LeaveSquad();
			}
		}
		return;
	}
	const bool bMeaningful =
	    Existing->bAlive != Status.bAlive || Existing->bMobile != Status.bMobile ||
	    Existing->bPlayerControlled != Status.bPlayerControlled ||
	    Existing->Feedback.OrderId != Status.Feedback.OrderId ||
	    Existing->Feedback.Revision != Status.Feedback.Revision || Existing->Feedback.State != Status.Feedback.State ||
	    Existing->Feedback.bReady != Status.Feedback.bReady ||
	    (Existing->MagazineAmmo == 0) != (Status.MagazineAmmo == 0) ||
	    (Existing->MagazineAmmo + Existing->ReserveAmmo == 0) != (Status.MagazineAmmo + Status.ReserveAmmo == 0) ||
	    Existing->bReloading != Status.bReloading || (Existing->Suppression >= 0.65f) != (Status.Suppression >= 0.65f);
	*Existing = Status;
	if (bMeaningful)
	{
		Wake();
	}
}

void UDemoSquadContextComponent::ReportContact(const FDemoSquadContact& Contact)
{
	check(IsInGameThread());
	const double Now = GetWorld()->GetTimeSeconds();
	if (bEnding || !GetOwner()->HasAuthority() || !FindMember(Contact.ObserverUnitId) ||
	    Contact.Location.ContainsNaN() || !FMath::IsFinite(Contact.ObservedAt) || Contact.ObservedAt < 0.0 ||
	    Contact.ObservedAt > Now + 0.1 || !FMath::IsFinite(Contact.Confidence) ||
	    !FMath::IsFinite(Contact.UncertaintyRadius) || Contact.UncertaintyRadius < 0.0f)
	{
		return;
	}
	FDemoSquadContact* Existing = Contacts.FindByPredicate(
	    [&Contact](const auto& Entry)
	    {
		    return (Contact.ContactId.IsValid() && Entry.ContactId == Contact.ContactId) ||
		           (Contact.Actor.IsValid() && Entry.Actor == Contact.Actor);
	    });
	if (Existing && Contact.ObservedAt <= Existing->ObservedAt)
	{
		return;
	}
	const bool bMeaningful = !Existing || Contact.ObservedAt - Existing->ObservedAt > 2.0 ||
	                         FVector::DistSquared2D(Contact.Location, Existing->Location) > FMath::Square(500.0f);
	if (!Existing)
	{
		if (Contacts.Num() >= 64)
		{
			return;
		}
		Existing = &Contacts.AddDefaulted_GetRef();
		Existing->ContactId = Contact.ContactId.IsValid() ? Contact.ContactId : FGuid::NewGuid();
	}
	const FGuid Id = Existing->ContactId;
	*Existing = Contact;
	Existing->ContactId = Id;
	Existing->Confidence = FMath::Clamp(Contact.Confidence, 0.0f, 1.0f);
	if (bMeaningful)
	{
		Wake();
	}
}

bool UDemoSquadContextComponent::SetMission(const FDemoSquadMission& InMission)
{
	check(IsInGameThread());
	ReconcileMissionSource();
	return !bHasMissionSource && ApplyMission(InMission);
}

int32 UDemoSquadContextComponent::ClaimMissionSource(AActor* Source)
{
	check(IsInGameThread());
	ReconcileMissionSource();
	if (bEnding || !GetOwner()->HasAuthority() || GetWorld()->GetNetMode() == NM_Client || !IsValid(Source) ||
	    Source->IsActorBeingDestroyed() || Source->GetWorld() != GetWorld() || !Source->HasAuthority() ||
	    CommandMode == EDemoSquadCommandMode::PlayerManual || MissionSourceEpoch == MAX_int32 ||
	    (bHasMissionSource && MissionSource.Get() != Source))
	{
		return 0;
	}
	if (!bHasMissionSource)
	{
		MissionSource = Source;
		bHasMissionSource = true;
		++MissionSourceEpoch;
	}
	return MissionSourceEpoch;
}

bool UDemoSquadContextComponent::ReleaseMissionSource(AActor* Source, int32 Epoch)
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority() || !bHasMissionSource || MissionSource.Get() != Source ||
	    Epoch != MissionSourceEpoch)
	{
		return false;
	}
	CancelSourceMission();
	MissionSource.Reset();
	bHasMissionSource = false;
	return true;
}

bool UDemoSquadContextComponent::SetMissionFromSource(AActor* Source, int32 Epoch, const FDemoSquadMission& InMission)
{
	check(IsInGameThread());
	ReconcileMissionSource();
	if (!bHasMissionSource || MissionSource.Get() != Source || Epoch != MissionSourceEpoch ||
	    CommandMode == EDemoSquadCommandMode::PlayerManual || !ApplyMission(InMission))
	{
		return false;
	}
	bMissionFromSource = true;
	return true;
}

bool UDemoSquadContextComponent::CancelMissionFromSource(AActor* Source, int32 Epoch, FGuid MissionId, int32 Revision)
{
	check(IsInGameThread());
	if (bEnding || !GetOwner()->HasAuthority() || !bHasMissionSource || MissionSource.Get() != Source ||
	    Epoch != MissionSourceEpoch || !bMissionFromSource || MissionId != Mission.MissionId ||
	    Revision != Mission.Revision)
	{
		return false;
	}
	CancelSourceMission();
	return true;
}

AActor* UDemoSquadContextComponent::GetMissionSource() const
{
	return MissionSource.Get();
}

FDemoSquadSituationReport UDemoSquadContextComponent::GetSituationReport() const
{
	FDemoSquadSituationReport Report;
	Report.SquadId = SquadId;
	Report.TeamId = Members.IsEmpty() ? 255 : Members[0].TeamId;
	Report.ReportSequence = ReportSequence;
	Report.ObservedAt = SnapshotObservedAt;
	Report.bCommandAvailable = !bEnding && ExecutionDriver.IsValid() && IsLeadershipReady();
	Report.CommandMode = CommandMode;
	Report.MembershipEpoch = MissionSourceEpoch;
	Report.MissionResult = MissionResult;
	Report.Capability = Snapshot;
	Report.TaskSequence = TaskSequence;
	Report.CommandEpoch = CommandEpoch;
	Report.GateVersion = ExecutionGateVersion;
	Report.bReady = bExecutionPermitted && !Plan.Assignments.IsEmpty() && PhaseSatisfied();
	Report.Location = ChooseRallyPosition();
	return Report;
}

void UDemoSquadContextComponent::CancelSourceMission()
{
	if (bMissionFromSource)
	{
		bMissionFromSource = false;
		bMissionCompleted = true;
		CancelPlan();
		if (MissionResult.State == EDemoSquadMissionState::Running)
		{
			SetMissionOutcome(EDemoSquadMissionState::Cancelled);
		}
		Wake();
	}
}

void UDemoSquadContextComponent::ReconcileMissionSource()
{
	if (bHasMissionSource && (!MissionSource.IsValid() || MissionSource->IsActorBeingDestroyed()))
	{
		CancelSourceMission();
		MissionSource.Reset();
		bHasMissionSource = false;
	}
}

bool UDemoSquadContextComponent::ApplyMission(const FDemoSquadMission& InMission)
{
	check(IsInGameThread());
	if (bEnding || !GetOwner()->HasAuthority() || InMission.Revision < 1 ||
	    !Nelaric::Squad::ValidMissionArea(InMission.Goal) || !Nelaric::Squad::ValidMissionArea(InMission.Fallback) ||
	    !Nelaric::Squad::ValidMissionArea(InMission.Boundary) || InMission.Facing.ContainsNaN() ||
	    !FMath::IsFinite(InMission.Deadline) ||
	    static_cast<uint8>(InMission.Type) > static_cast<uint8>(EDemoSquadMissionType::Regroup) ||
	    static_cast<uint8>(InMission.Engagement) > static_cast<uint8>(EDemoSquadEngagement::FireAtWill) ||
	    (InMission.MissionId.IsValid() && InMission.MissionId == Mission.MissionId &&
	     InMission.Revision <= Mission.Revision))
	{
		return false;
	}
	CancelPlan();
	Mission = InMission;
	if (!Mission.MissionId.IsValid())
	{
		Mission.MissionId = FGuid::NewGuid();
	}
	bMissionCompleted = false;
	ConsecutivePlanFailures = 0;
	bExecutionPermitted = true;
	ExecutionGateVersion = 0;
	SetMissionOutcome(Mission.Type == EDemoSquadMissionType::None ? EDemoSquadMissionState::Cancelled
	                                                              : EDemoSquadMissionState::Running);
	Wake();
	return true;
}

void UDemoSquadContextComponent::SetCommandMode(EDemoSquadCommandMode Mode)
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority() || CommandMode == Mode ||
	    static_cast<uint8>(Mode) > static_cast<uint8>(EDemoSquadCommandMode::NoCommander))
	{
		return;
	}
	if ((Mode == EDemoSquadCommandMode::NoCommander || Mode == EDemoSquadCommandMode::Suspended))
	{
		LoseLeadership();
	}
	else
	{
		CancelPlan();
		++CommandEpoch;
	}
	CommandMode = Mode == EDemoSquadCommandMode::NoCommander ? EDemoSquadCommandMode::Suspended : Mode;
	Wake();
}

bool UDemoSquadContextComponent::SetManualTactic(EDemoSquadTactic Tactic)
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority() || CommandMode != EDemoSquadCommandMode::PlayerManual ||
	    (Tactic != EDemoSquadTactic::Idle && MissionResult.State != EDemoSquadMissionState::Running) ||
	    static_cast<uint8>(Tactic) > static_cast<uint8>(EDemoSquadTactic::Regroup))
	{
		return false;
	}
	ManualTactic = Tactic;
	CancelPlan();
	Wake();
	return true;
}

FDemoSquadMission UDemoSquadContextComponent::GetMission() const
{
	return Mission;
}
FDemoSquadMissionResult UDemoSquadContextComponent::GetMissionResult() const
{
	return MissionResult;
}

void UDemoSquadContextComponent::SetMissionOutcome(EDemoSquadMissionState State, EDemoSquadFailure Failure)
{
	MissionResult.MissionId = Mission.MissionId;
	MissionResult.Revision = Mission.Revision;
	MissionResult.State = State;
	MissionResult.Failure = Failure;
	MissionResult.ReportedAt = GetWorld()->GetTimeSeconds();
	++TaskSequence;
}

bool UDemoSquadContextComponent::ConfirmMissionCompleted(FGuid MissionId, int32 Revision)
{
	check(IsInGameThread());
	if (GetOwner()->HasAuthority() && !bEnding && MissionId == Mission.MissionId && Revision == Mission.Revision &&
	    MissionResult.State == EDemoSquadMissionState::Succeeded &&
	    (Mission.Type == EDemoSquadMissionType::Defend || Mission.Type == EDemoSquadMissionType::Control))
		return true;
	if (!GetOwner()->HasAuthority() || bEnding || MissionId != Mission.MissionId || Revision != Mission.Revision ||
	    MissionResult.State != EDemoSquadMissionState::Running)
	{
		return false;
	}
	bMissionCompleted = true;
	SetMissionOutcome(EDemoSquadMissionState::Succeeded);
	if (Mission.Type != EDemoSquadMissionType::Defend && Mission.Type != EDemoSquadMissionType::Control)
	{
		CancelPlan();
	}
	Wake();
	return true;
}

FDemoSquadPlan UDemoSquadContextComponent::GetPlan() const
{
	return Plan;
}
FDemoSquadSnapshot UDemoSquadContextComponent::GetSnapshot() const
{
	return Snapshot;
}
TArray<FDemoSquadMemberStatus> UDemoSquadContextComponent::GetMembers() const
{
	return Members;
}
FGuid UDemoSquadContextComponent::GetSquadId() const
{
	return SquadId;
}
FGuid UDemoSquadContextComponent::GetLeaderUnitId() const
{
	return {};
}
EDemoSquadCommandMode UDemoSquadContextComponent::GetCommandMode() const
{
	return CommandMode;
}
Nelaric::Squad::FChanged& UDemoSquadContextComponent::OnChanged()
{
	return Changed;
}

TArray<FDemoSquadContact> UDemoSquadContextComponent::GetContacts() const
{
	TArray<FDemoSquadContact> Result = Contacts;
	const double Now = GetWorld()->GetTimeSeconds();
	for (auto& Contact : Result)
	{
		const float Age = FMath::Max(0.0f, float(Now - Contact.ObservedAt));
		Contact.Confidence = FMath::Max(0.0f, Contact.Confidence - Age * GetTactics().ContactDecay);
		Contact.UncertaintyRadius += Age * GetTactics().ContactUncertaintyGrowth;
	}
	return Result;
}

bool UDemoSquadContextComponent::IsCurrentIdentity(const FDemoSquadRequestIdentity& Identity) const
{
	return !bEnding && GetWorld() && !GetWorld()->bIsTearingDown && !GetOwner()->IsActorBeingDestroyed() &&
	       Identity.CommandEpoch == CommandEpoch && Identity.MissionId == Mission.MissionId &&
	       Identity.MissionRevision == Mission.Revision && Identity.PlanId == Plan.Identity.PlanId &&
	       Identity.Generation == RequestGeneration && Identity.PlanId.IsValid();
}

bool UDemoSquadContextComponent::StartExecution(UObject* Driver)
{
	check(IsInGameThread());
	if (bEnding || !GetOwner()->HasAuthority() || !IsValid(Driver) ||
	    !Nelaric::Squad::ValidConfiguration(GetDefinition(), GetTactics()) ||
	    !Members.ContainsByPredicate([](const auto& Member) { return Member.bAlive; }) ||
	    (ExecutionDriver.IsValid() && ExecutionDriver.Get() != Driver))
	{
		return false;
	}
	if (ExecutionDriver.IsValid())
	{
		return ExecutionDriver.Get() == Driver;
	}
	ExecutionDriver = Driver;
	RebuildSnapshot();
	Wake();
	return true;
}

void UDemoSquadContextComponent::StopExecution(UObject* Driver)
{
	if (Driver && ExecutionDriver.Get() != Driver)
	{
		return;
	}
	ExecutionDriver.Reset();
	GetWorld()->GetTimerManager().ClearTimer(UpdateTimer);
	CancelPlan();
}

bool UDemoSquadContextComponent::IsLeadershipReady() const
{
	return !bEnding && GetOwner()->HasAuthority() && ExecutionDriver.IsValid() &&
	       CommandMode != EDemoSquadCommandMode::Suspended && CommandMode != EDemoSquadCommandMode::NoCommander;
}

void UDemoSquadContextComponent::LoseLeadership()
{
	++CommandEpoch;
	DegradeOrders();
	Wake();
}

bool UDemoSquadContextComponent::ResolveLeadership()
{
	return IsLeadershipReady();
}

bool UDemoSquadContextComponent::SetExecutionPermitFromSource(AActor* Source, int32 Epoch, FGuid MissionId,
                                                              int32 Revision, int32 GateVersion, bool bAllowed)
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority() || bEnding || Source != MissionSource.Get() || Epoch != MissionSourceEpoch ||
	    MissionId != Mission.MissionId || Revision != Mission.Revision || GateVersion < ExecutionGateVersion ||
	    GateVersion < 1)
		return false;
	if (GateVersion == ExecutionGateVersion)
		return bAllowed == bExecutionPermitted;
	ExecutionGateVersion = GateVersion;
	bExecutionPermitted = bAllowed;
	if (!bAllowed)
		DegradeOrders();
	Wake();
	return true;
}

bool UDemoSquadContextComponent::IsExecutionPermitted() const
{
	return bExecutionPermitted;
}

bool UDemoSquadContextComponent::RestoreIdentity(FGuid Identity, bool bApply)
{
	check(IsInGameThread());
	if (!GetOwner()->HasAuthority() || bEnding || !Identity.IsValid())
		return false;
	for (TActorIterator<ADemoSquadCommandActor> It(GetWorld()); It; ++It)
		if (*It != GetOwner() && It->GetSquadContext()->GetSquadId() == Identity)
			return false;
	if (!bApply || SquadId == Identity)
		return true;
	if (ExecutionDriver.IsValid())
		DegradeOrders();
	SquadId = Identity;
	Wake();
	return true;
}

void UDemoSquadContextComponent::RebuildSnapshot()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Squad_Snapshot);
	ReconcileMissionSource();
	Snapshot = {};
	const double Now = GetWorld()->GetTimeSeconds();
	const FVector IntegrityAnchor = Plan.Assignments.IsEmpty() ? ChooseRallyPosition() : Plan.FormationAnchor;
	SnapshotObservedAt = Members.IsEmpty() ? -1.0 : Now;
	for (const auto& Member : Members)
		SnapshotObservedAt = FMath::Min(SnapshotObservedAt, Member.ReportedAt);
	if (ReportSequence < MAX_int32)
	{
		++ReportSequence;
	}
	int32 Suppressed = 0;
	int32 Armed = 0;
	int32 NearAnchor = 0;
	for (const auto& Member : Members)
	{
		if (!Member.bAlive || !Member.bMobile || Member.bPlayerControlled ||
		    Now - Member.ReportedAt > GetTactics().FeedbackTimeoutSeconds)
		{
			continue;
		}
		++Snapshot.EffectiveMemberCount;
		++Snapshot.MobileMemberCount;
		Suppressed += Member.Suppression >= 0.65f ? 1 : 0;
		Armed += Member.MagazineAmmo + Member.ReserveAmmo > 0 ? 1 : 0;
		NearAnchor += FVector::DistSquared2D(Member.Location, IntegrityAnchor) < FMath::Square(1500.0f) ? 1 : 0;
		const auto* Assignment =
		    Plan.Assignments.FindByPredicate([&Member](const auto& Entry) { return Entry.UnitId == Member.UnitId; });
		if (Assignment && Assignment->Order.Type == EDemoSquadOrderType::SupportSector &&
		    Member.Feedback.OrderId == Assignment->Order.OrderId &&
		    Member.Feedback.Revision == Assignment->Order.Revision &&
		    Member.Feedback.State == EDemoSquadOrderState::Executing && Member.Feedback.bReady)
		{
			++Snapshot.ReadySupportCount;
		}
	}
	if (Snapshot.EffectiveMemberCount > 0)
	{
		Snapshot.SuppressedMemberRatio = float(Suppressed) / Snapshot.EffectiveMemberCount;
		Snapshot.AmmoReadiness = float(Armed) / Snapshot.EffectiveMemberCount;
		Snapshot.FormationIntegrity = float(NearAnchor) / Snapshot.EffectiveMemberCount;
	}
	for (const auto& Contact : GetContacts())
	{
		Snapshot.KnownThreatPressure =
		    FMath::Clamp(Snapshot.KnownThreatPressure + Contact.Confidence * 0.35f, 0.0f, 1.0f);
	}
	Snapshot.RouteConfidence = Plan.Route.IsEmpty() ? 0.0f : 1.0f;
	Contacts.RemoveAll(
	    [this, Now](const auto& Contact)
	    { return Contact.Confidence - float(Now - Contact.ObservedAt) * GetTactics().ContactDecay <= 0.0f; });
}

EDemoSquadTactic UDemoSquadContextComponent::GetManualTactic() const
{
	return ManualTactic;
}

FVector UDemoSquadContextComponent::ChooseRallyPosition() const
{
	const bool bHasAutomatic = Members.ContainsByPredicate(
	    [](const auto& Member) { return Member.bAlive && Member.bMobile && !Member.bPlayerControlled; });
	// Choose an existing mobile member minimizing bounded total distance.
	// This medoid cannot be pulled into empty space by a single distant member.
	const FDemoSquadMemberStatus* Best = nullptr;
	double BestCost = TNumericLimits<double>::Max();
	for (const auto& Candidate : Members)
	{
		if (!Candidate.bAlive || !Candidate.bMobile || (bHasAutomatic && Candidate.bPlayerControlled))
		{
			continue;
		}
		double Cost = 0.0;
		for (const auto& Other : Members)
		{
			if (Other.bAlive && Other.bMobile && (!bHasAutomatic || !Other.bPlayerControlled))
			{
				Cost += FMath::Min(3000.0, FVector::Dist2D(Candidate.Location, Other.Location));
			}
		}
		if (Cost < BestCost)
		{
			BestCost = Cost;
			Best = &Candidate;
		}
	}
	return Best ? Best->Location : GetOwner()->GetActorLocation();
}

FDemoSquadArea UDemoSquadContextComponent::ResolveGoal(EDemoSquadGoalSource Source) const
{
	FDemoSquadArea Goal = Mission.Goal;
	if (Source == EDemoSquadGoalSource::Fallback)
	{
		Goal = Mission.Fallback;
	}
	else if (Source == EDemoSquadGoalSource::Rally)
	{
		Goal.Center = ChooseRallyPosition();
		Goal.Radius = FMath::Max(500.0f, GetTactics().SlotSpacing * 2.0f);
	}
	else if (Source == EDemoSquadGoalSource::Contact)
	{
		const auto Intelligence = GetContacts();
		const FDemoSquadContact* Best = nullptr;
		for (const auto& Contact : Intelligence)
		{
			if (Contact.Confidence > 0.15f && (!Best || Contact.ObservedAt > Best->ObservedAt))
			{
				Best = &Contact;
			}
		}
		if (Best)
		{
			Goal.Center = Best->Location;
			Goal.Radius = FMath::Clamp(Best->UncertaintyRadius + 300.0f, 300.0f, 2000.0f);
		}
	}
	return Goal;
}

void UDemoSquadContextComponent::CancelPlan(bool bKeepSnapshot)
{
	PolicyGoals.Reset();
	PolicyRetryAt.Reset();
	NextPolicyRefreshAt = 0.0;
	if (Plan.Identity.PlanId.IsValid() && !Plan.Assignments.IsEmpty() && PlanCancellationCount < MAX_int32)
		++PlanCancellationCount;
	++RequestGeneration;
	UDemoSquadPlanningSubsystem* Service = GetWorld()->GetSubsystem<UDemoSquadPlanningSubsystem>();
	const FGuid OldRequest = RouteRequestId;
	RouteRequestId.Invalidate();
	if (Service)
	{
		Service->CancelRoute(OldRequest);
	}
	for (const auto& Assignment : Plan.Assignments)
	{
		if (auto* Receiver = Nelaric::Squad::Receiver(FindMember(Assignment.UnitId)))
		{
			Receiver->CancelOrder(Assignment.Order.OrderId);
		}
		if (Service)
		{
			Service->ReleasePosition(Assignment.PositionId, Plan.Identity.PlanId);
		}
	}
	if (!bKeepSnapshot)
	{
		Plan = {};
	}
	bPhaseDispatched = false;
}

bool UDemoSquadContextComponent::BuildPlan(EDemoSquadTactic Tactic, EDemoSquadGoalSource GoalSource, bool bSplitSupport,
                                           int32 SupportGroupSize)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Squad_PlanBuild);
	check(IsInGameThread());
	if (!IsLeadershipReady() || !bExecutionPermitted || !ExecutionDriver.IsValid())
	{
		return false;
	}
	if (IsCurrentIdentity(Plan.Identity) && Plan.Tactic == Tactic &&
	    (Plan.Phase != EDemoSquadPhase::Completed || Mission.Type == EDemoSquadMissionType::Defend ||
	     Mission.Type == EDemoSquadMissionType::Control) &&
	    Plan.Phase != EDemoSquadPhase::Failed)
	{
		return true;
	}
	CancelPlan();
	Plan.Identity.CommandEpoch = CommandEpoch;
	Plan.Identity.MissionId = Mission.MissionId;
	Plan.Identity.MissionRevision = Mission.Revision;
	Plan.Identity.PlanId = FGuid::NewGuid();
	Plan.Identity.Generation = RequestGeneration;
	Plan.Tactic = Tactic;
	Plan.StartedAt = GetWorld()->GetTimeSeconds();
	Plan.FormationAnchor = ChooseRallyPosition();
	Plan.Reason = TEXT("Authored StateTree plan request");
	bSplitGroups = bSplitSupport && !UsesMemberOrderPolicy();
	TArray<FDemoSquadMemberStatus> Sorted = Members;
	Sorted.Sort(
	    [](const auto& Left, const auto& Right)
	    {
		    const bool LeftSupport = Left.Role == EDemoSquadRole::Support;
		    const bool RightSupport = Right.Role == EDemoSquadRole::Support;
		    if (LeftSupport != RightSupport)
		    {
			    return LeftSupport;
		    }
		    return Left.UnitId.ToString() < Right.UnitId.ToString();
	    });
	int32 Support = 0;
	const int32 SupportLimit =
	    FMath::Min(FMath::Clamp(SupportGroupSize, 1, 32), FMath::Max(0, Snapshot.EffectiveMemberCount - 1));
	for (const auto& Member : Sorted)
	{
		if (!Member.bAlive || !Member.Character.IsValid())
		{
			continue;
		}
		FDemoSquadAssignment Assignment;
		Assignment.UnitId = Member.UnitId;
		Assignment.BindingGeneration = Member.BindingGeneration;
		Assignment.bGuaranteed = Member.bMobile && !Member.bPlayerControlled;
		Assignment.bRequired = Member.bRequired;
		Assignment.Order.UnitId = Member.UnitId;
		Assignment.Order.SquadId = SquadId;
		Assignment.Order.Identity = Plan.Identity;
		Assignment.Order.OrderId = FGuid::NewGuid();
		Assignment.Order.SlotId = Plan.Assignments.Num();
		Assignment.Order.Engagement = Mission.Engagement;
		Assignment.Order.Facing = Mission.Facing.GetSafeNormal2D();
		Assignment.Order.IssuedAt = GetWorld()->GetTimeSeconds();
		if (bSplitGroups && Assignment.bGuaranteed && Member.WeaponRange > 0.0f &&
		    Member.MagazineAmmo + Member.ReserveAmmo > 0 && Support < SupportLimit)
		{
			Assignment.Order.Group = 1;
			++Support;
		}
		Plan.Assignments.Add(Assignment);
	}
	if (bSplitGroups && Support == 0)
	{
		FailPlan(EDemoSquadFailure::NoAmmo);
		return false;
	}
	FDemoSquadArea Goal = UsesMemberOrderPolicy() ? Mission.Goal : ResolveGoal(GoalSource);
	Plan.Goal = Goal;
	if (UsesMemberOrderPolicy())
	{
		Plan.Route.Add(Goal.Center);
		Plan.RouteIndex = 0;
		SetPhase(EDemoSquadPhase::None);
		return true;
	}
	const FDemoSquadMemberStatus* Agent = Members.FindByPredicate(
	    [](const auto& Member)
	    { return Member.bAlive && Member.bMobile && !Member.bPlayerControlled && Member.Character.IsValid(); });
	if (!Agent)
	{
		Agent = Members.FindByPredicate([](const auto& Member)
		                                { return Member.bAlive && Member.bMobile && Member.Character.IsValid(); });
	}
	UDemoSquadPlanningSubsystem* Service = GetWorld()->GetSubsystem<UDemoSquadPlanningSubsystem>();
	if (!Agent || !Service || (Mission.bHasBoundary && !Nelaric::Squad::Inside(Mission.Boundary, Goal.Center)))
	{
		FailPlan(EDemoSquadFailure::Unreachable);
		return false;
	}
	SetPhase(EDemoSquadPhase::Planning);
	const FDemoSquadRequestIdentity Identity = Plan.Identity;
	Nelaric::Squad::FRouteFinished Callback;
	Callback.BindWeakLambda(this, [this, Identity](EDemoSquadFailure Failure, const TArray<FVector>& Points)
	                        { HandleRoute(Identity, Failure, Points); });
	if (RouteRequestCount < MAX_int32)
		++RouteRequestCount;
	RouteRequestId = Service->QueueRoute(Agent->Character.Get(), Plan.FormationAnchor, Goal.Center, MoveTemp(Callback));
	if (!RouteRequestId.IsValid())
	{
		FailPlan(EDemoSquadFailure::Unreachable);
		return false;
	}
	Wake();
	return true;
}

void UDemoSquadContextComponent::HandleRoute(FDemoSquadRequestIdentity Identity, EDemoSquadFailure Failure,
                                             const TArray<FVector>& Points)
{
	check(IsInGameThread());
	if (!IsCurrentIdentity(Identity) || !IsLeadershipReady() || !ExecutionDriver.IsValid())
	{
		return;
	}
	RouteRequestId.Invalidate();
	if (Failure != EDemoSquadFailure::None || Points.IsEmpty())
	{
		FailPlan(Failure == EDemoSquadFailure::None ? EDemoSquadFailure::Unreachable : Failure);
		return;
	}
	Plan.Route = Points;
	Plan.RouteIndex = Points.Num() > 1 ? 1 : 0;
	Plan.FormationAnchor = Points[0];
	Plan.FormationDirection =
	    Points.Num() > 1 ? (Points[1] - Points[0]).GetSafeNormal2D() : Mission.Facing.GetSafeNormal2D();
	SetPhase(EDemoSquadPhase::None);
	Wake();
}

void UDemoSquadContextComponent::SetPhase(EDemoSquadPhase Phase)
{
	Plan.Phase = Phase;
	Plan.PhaseStartedAt = GetWorld()->GetTimeSeconds();
	Plan.PhaseDeadline = 0.0;
	bPhaseDispatched = false;
	Wake();
}

bool UDemoSquadContextComponent::BeginPhase(const FDemoSquadPhaseSettings& Settings)
{
	if (!IsLeadershipReady() || !IsCurrentIdentity(Plan.Identity) || Plan.Route.IsEmpty() ||
	    !FMath::IsFinite(Settings.TimeoutSeconds) || Settings.TimeoutSeconds < 0.0f ||
	    !FMath::IsFinite(Settings.ArrivalRatio) || Settings.ArrivalRatio <= 0.0f || Settings.ArrivalRatio > 1.0f ||
	    !FMath::IsFinite(Settings.LocalSectorRadius) || Settings.LocalSectorRadius < 50.0f)
	{
		return false;
	}
	PhaseSettings = Settings;
	if (UsesMemberOrderPolicy())
		PhaseSettings.TimeoutSeconds = 0.0f;
	SetPhase(Settings.Phase);
	Plan.PhaseDeadline = PhaseSettings.TimeoutSeconds > 0.0f ? Plan.PhaseStartedAt + PhaseSettings.TimeoutSeconds : 0.0;
	if (!AssignPositions())
	{
		FailPlan(EDemoSquadFailure::PositionUnavailable);
		return false;
	}
	PublishOrders(true);
	return Plan.Phase != EDemoSquadPhase::Failed;
}

bool UDemoSquadContextComponent::AssignPosition(FDemoSquadAssignment& Assignment, FVector Center)
{
	UDemoSquadPlanningSubsystem* Service = GetWorld()->GetSubsystem<UDemoSquadPlanningSubsystem>();
	if (!Service)
	{
		return false;
	}
	const float Spacing = FMath::Max(120.0f, GetTactics().SlotSpacing);
	FVector Forward = Plan.FormationDirection.IsNearlyZero() ? FVector::ForwardVector : Plan.FormationDirection;
	const FVector Right(-Forward.Y, Forward.X, 0.0);
	const int32 Slot = FMath::Max(0, Assignment.Order.SlotId);
	FVector Offset = Right * ((Slot % 2 == 0 ? -1.0f : 1.0f) * Spacing * 0.5f) - Forward * (Slot / 2) * Spacing;
	const bool bFinalArea = !PhaseSettings.bFollowRoute || Plan.RouteIndex + 1 >= Plan.Route.Num();
	if (bFinalArea && !(bSplitGroups && Assignment.Order.Group != 0))
	{
		const float Angle = 2.0f * PI * Slot / FMath::Max(1, Plan.Assignments.Num());
		// Small platoon sectors must still provide distinct slots, rather than
		// collapsing every soldier onto a single reserved position.
		const float RingRadius = FMath::Max(0.0f, Plan.Goal.Radius - 40.0f);
		Offset = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * RingRadius;
	}
	FVector Position;
	const FGuid Lease = Service->ReservePosition(Assignment.UnitId, Plan.Identity.PlanId,
	                                             Center + Offset + Right * Assignment.RetryCount * Spacing, Position);
	if (!Lease.IsValid() || (Mission.bHasBoundary && !Nelaric::Squad::Inside(Mission.Boundary, Position)))
	{
		Service->ReleasePosition(Lease, Plan.Identity.PlanId);
		return false;
	}
	Service->ReleasePosition(Assignment.PositionId, Plan.Identity.PlanId);
	Assignment.PositionId = Lease;
	Assignment.Order.Goal.Center = Position;
	Assignment.Order.Goal.Radius =
	    PhaseSettings.OrderType == EDemoSquadOrderType::SearchArea
	        ? FMath::Max(150.0f, Plan.Goal.Radius / FMath::Sqrt(float(FMath::Max(1, Plan.Assignments.Num()))))
	        : FMath::Max(120.0f, Spacing * 0.75f);
	Assignment.Order.MovementArea = Mission.bHasBoundary ? Mission.Boundary : Plan.Goal;
	if (!Mission.bHasBoundary)
	{
		const FVector Start = Plan.Route.IsEmpty() ? Plan.FormationAnchor : Plan.Route[0];
		const FVector End = Plan.Route.IsEmpty() ? Center : Plan.Route.Last();
		Assignment.Order.MovementArea.Center = (Start + End) * 0.5;
		float Radius = 0.0f;
		for (const FVector& Point : Plan.Route)
		{
			Radius = FMath::Max(Radius, float(FVector::Dist2D(Point, Assignment.Order.MovementArea.Center)));
		}
		Assignment.Order.MovementArea.Radius = Radius + Spacing * (Plan.Assignments.Num() + 2);
	}
	return true;
}

bool UDemoSquadContextComponent::UsesMemberOrderPolicy() const
{
	return MemberOrderHandler.IsBound() &&
	       (Mission.Type == EDemoSquadMissionType::Control || Mission.Type == EDemoSquadMissionType::Defend);
}

bool UDemoSquadContextComponent::UpdatePolicyOrder(FDemoSquadAssignment& Assignment)
{
	const auto* Member = FindMember(Assignment.UnitId);
	auto* Service = GetWorld()->GetSubsystem<UDemoSquadPlanningSubsystem>();
	if (!Member || !Member->bAlive || !Member->bMobile || Member->bPlayerControlled || !Member->Character.IsValid())
		return true;
	if (!Service || !bExecutionPermitted || !UsesMemberOrderPolicy())
		return false;
	const auto InputOrder = Assignment.Order;
	auto Proposed = MemberOrderHandler.Execute(Member->Character.Get(), InputOrder);
	if (Proposed.Goal.Center.ContainsNaN() || Proposed.MovementArea.Center.ContainsNaN() ||
	    !FMath::IsFinite(Proposed.MovementArea.Radius) || Proposed.MovementArea.Radius <= 0.0f ||
	    !Nelaric::Squad::Inside(Proposed.MovementArea, Proposed.Goal.Center))
		return false;
	const FVector* PreviousGoal = PolicyGoals.Find(Assignment.UnitId);
	const bool bFailed =
	    Member->Feedback.OrderId == Assignment.Order.OrderId && Member->Feedback.State == EDemoSquadOrderState::Failed;
	if (PreviousGoal && PreviousGoal->Equals(Proposed.Goal.Center, 1.0f) && Assignment.PositionId.IsValid() &&
	    !bFailed && Assignment.Order.bAllowStopToFight == Proposed.bAllowStopToFight)
	{
		Service->RenewPosition(Assignment.PositionId, Plan.Identity.PlanId);
		return true;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Retry = PolicyRetryAt.Find(Assignment.UnitId); Retry && Now < *Retry)
		return Assignment.PositionId.IsValid();
	auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	auto* Body = Member->Character.Get();
	const FVector Start = Body->GetNavAgentLocation();
	const auto* Data = Nav ? Nav->GetNavDataForProps(Body->GetNavAgentPropertiesRef(), Start) : nullptr;
	if (!Data)
		return false;
	FVector Position = FVector::ZeroVector;
	FGuid Lease;
	for (int32 Attempt = 0; Attempt < 9 && !Lease.IsValid(); ++Attempt)
	{
		const float Angle = Attempt * PI * 0.25f;
		const FVector Offset =
		    Attempt == 0 ? FVector::ZeroVector : FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * 140.0f;
		FNavLocation Destination;
		if (!Nav->ProjectPointToNavigation(Proposed.Goal.Center + Offset, Destination, FVector(200, 200, 6000), Data) ||
		    !Nelaric::Squad::Inside(Proposed.MovementArea, Destination.Location))
			continue;
		FPathFindingQuery Query(Body, *Data, Start, Destination.Location);
		Query.SetAllowPartialPaths(false);
		if (!Nav->TestPathSync(Query, EPathFindingMode::Regular))
			continue;
		Lease = Service->ReservePosition(Assignment.UnitId, Plan.Identity.PlanId, Destination.Location, Position);
		if (Lease.IsValid() && !Nelaric::Squad::Inside(Proposed.MovementArea, Position))
		{
			Service->ReleasePosition(Lease, Plan.Identity.PlanId);
			Lease.Invalidate();
		}
	}
	if (!Lease.IsValid())
	{
		PolicyRetryAt.Add(Assignment.UnitId, Now + 0.5);
		return Assignment.PositionId.IsValid();
	}
	PolicyRetryAt.Remove(Assignment.UnitId);
	Service->ReleasePosition(Assignment.PositionId, Plan.Identity.PlanId);
	Assignment.PositionId = Lease;
	PolicyGoals.Add(Assignment.UnitId, Proposed.Goal.Center);
	auto& Order = Assignment.Order;
	Order.OrderId = FGuid::NewGuid();
	++Order.Revision;
	Order.Type = EDemoSquadOrderType::MaintainFormation;
	Order.Group = 0;
	Order.Goal = FDemoSquadArea(Position, 80.0f);
	Order.AcceptanceRadius = 60.0f;
	Order.MovementArea = Proposed.MovementArea;
	Order.bAllowStopToFight = Proposed.bAllowStopToFight;
	Order.bAllowLocalReposition = false;
	Order.bHasFocus = false;
	Order.Deadline = 0.0;
	Order.ExpiresAt = 0.0;
	Order.IssuedAt = GetWorld()->GetTimeSeconds();
	return true;
}

bool UDemoSquadContextComponent::AssignPositions()
{
	const FVector Destination = PhaseSettings.bFollowRoute ? Plan.Route[Plan.RouteIndex] : Plan.Goal.Center;
	for (auto& Assignment : Plan.Assignments)
	{
		if (UsesMemberOrderPolicy())
		{
			UpdatePolicyOrder(Assignment);
			continue;
		}
		auto& Order = Assignment.Order;
		if (bSplitGroups && Order.Group != 0 && PhaseSettings.bPreserveSupport)
		{
			// Extend support in place; do not cancel its movement or firing task.
			if (Order.Deadline > 0.0)
			{
				Order.Deadline = 0.0;
				++Order.Revision;
			}
			continue;
		}
		const bool bSupport = bSplitGroups && Order.Group == 1;
		const bool bCoordination = bSplitGroups && Order.Group == 2;
		const bool bAtAnchor = bSupport || bCoordination || PhaseSettings.bHoldMovement;
		Order.Type = bSupport                      ? PhaseSettings.SupportOrderType
		             : bCoordination               ? PhaseSettings.CoordinationOrderType
		             : PhaseSettings.bHoldMovement ? PhaseSettings.HoldingOrderType
		                                           : PhaseSettings.OrderType;
		Order.bAllowStopToFight = PhaseSettings.bAllowStopToFight;
		Order.bAllowLocalReposition = PhaseSettings.bAllowLocalReposition;
		Order.bHasFocus = bSupport || PhaseSettings.bLocalSector;
		Order.FocusLocation = Plan.Goal.Center;
		Order.Deadline = Plan.PhaseDeadline;
		++Order.Revision;
		if (!AssignPosition(Assignment, bAtAnchor ? Plan.FormationAnchor : Destination))
		{
			return false;
		}
		if (PhaseSettings.bLocalSector || bAtAnchor)
		{
			Order.MovementArea = Order.Goal;
			Order.MovementArea.Radius = FMath::Max(Order.Goal.Radius, PhaseSettings.LocalSectorRadius);
			if (Mission.bHasBoundary)
			{
				const float Remaining =
				    Mission.Boundary.Radius - FVector::Dist2D(Mission.Boundary.Center, Order.Goal.Center);
				Order.MovementArea.Radius = FMath::Min(Order.MovementArea.Radius, Remaining);
				if (Order.MovementArea.Radius < 1.0f)
				{
					return false;
				}
			}
		}
		if (bAtAnchor)
		{
			Order.Facing = (Plan.Goal.Center - Order.Goal.Center).GetSafeNormal2D();
		}
	}
	return true;
}

void UDemoSquadContextComponent::PublishOrders(bool bOnlyChanged)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Squad_OrderDispatch);
	if (!IsLeadershipReady() || !IsCurrentIdentity(Plan.Identity))
	{
		return;
	}
	bool bAccepted = true;
	for (const auto& Assignment : Plan.Assignments)
	{
		// A temporarily crowded destination is retried without failing the squad.
		if (UsesMemberOrderPolicy() && !Assignment.PositionId.IsValid())
			continue;
		const auto* Member = FindMember(Assignment.UnitId);
		if (!Member || Member->BindingGeneration != Assignment.BindingGeneration || !Member->bAlive)
		{
			continue;
		}
		if (auto* Receiver = Nelaric::Squad::Receiver(Member))
		{
			const auto Existing = Receiver->GetOrder();
			if (!bOnlyChanged || Existing.OrderId != Assignment.Order.OrderId ||
			    Existing.Revision != Assignment.Order.Revision)
			{
				bAccepted &= Receiver->ReceiveOrder(Assignment.Order);
			}
		}
	}
	bPhaseDispatched = true;
	if (!bAccepted)
	{
		FailPlan(EDemoSquadFailure::InvalidOrder);
	}
}

bool UDemoSquadContextComponent::RequiredMembersAvailable() const
{
	for (const auto& Assignment : Plan.Assignments)
	{
		if (!Assignment.bGuaranteed || !Assignment.bRequired)
		{
			continue;
		}
		const auto* Member = FindMember(Assignment.UnitId);
		if (!Member || Member->BindingGeneration != Assignment.BindingGeneration || !Member->bAlive ||
		    !Member->bMobile || Member->bPlayerControlled)
		{
			return false;
		}
	}
	return true;
}

bool UDemoSquadContextComponent::PhaseSatisfied() const
{
	int32 Expected = 0;
	int32 Ready = 0;
	const double Now = GetWorld()->GetTimeSeconds();
	for (const auto& Assignment : Plan.Assignments)
	{
		if (!Assignment.bGuaranteed)
		{
			continue;
		}
		const bool bSupport = Assignment.Order.Group == 1 && bSplitGroups;
		if ((PhaseSettings.Readiness == EDemoSquadReadiness::Support && !bSupport) ||
		    (PhaseSettings.Readiness == EDemoSquadReadiness::Maneuver && Assignment.Order.Group != 0))
		{
			continue;
		}
		++Expected;
		const auto* Member = FindMember(Assignment.UnitId);
		const bool bReady = Member && Member->BindingGeneration == Assignment.BindingGeneration && Member->bAlive &&
		                    Member->bMobile && !Member->bPlayerControlled &&
		                    Now - Member->ReportedAt <= GetTactics().FeedbackTimeoutSeconds &&
		                    Member->Feedback.OrderId == Assignment.Order.OrderId &&
		                    Member->Feedback.Revision == Assignment.Order.Revision && Member->Feedback.bReady;
		if (Assignment.bRequired && !bReady)
		{
			return false;
		}
		Ready += bReady ? 1 : 0;
	}
	if (PhaseSettings.Readiness == EDemoSquadReadiness::Support)
	{
		return Ready >= FMath::Clamp(PhaseSettings.MinimumReadySupport, 1, 32);
	}
	return Expected > 0 && Ready >= FMath::Max(1, FMath::CeilToInt(Expected * PhaseSettings.ArrivalRatio));
}

void UDemoSquadContextComponent::UpdateFormation()
{
	if (!PhaseSatisfied() || Plan.RouteIndex + 1 >= Plan.Route.Num())
	{
		return;
	}
	Plan.FormationAnchor = Plan.Route[Plan.RouteIndex];
	++Plan.RouteIndex;
	Plan.FormationDirection = (Plan.Route[Plan.RouteIndex] - Plan.FormationAnchor).GetSafeNormal2D();
	for (auto& Assignment : Plan.Assignments)
	{
		if (!AssignPosition(Assignment, Plan.Route[Plan.RouteIndex]))
		{
			FailPlan(EDemoSquadFailure::PositionUnavailable);
			return;
		}
		++Assignment.Order.Revision;
		Assignment.Order.Facing = Plan.FormationDirection;
	}
	PublishOrders(true);
}

void UDemoSquadContextComponent::FailPlan(EDemoSquadFailure Failure)
{
	if (Plan.Phase == EDemoSquadPhase::Failed)
	{
		return;
	}
	CancelPlan(true);
	Plan.Failure = Failure;
	Plan.Reason = TEXT("Execution failed; awaiting authored recovery transition");
	if (++ConsecutivePlanFailures >= 3 && MissionResult.State == EDemoSquadMissionState::Running)
	{
		bMissionCompleted = true;
		SetMissionOutcome(EDemoSquadMissionState::Failed, Failure);
	}
	SetPhase(EDemoSquadPhase::Failed);
}

void UDemoSquadContextComponent::MaintainPhase()
{
	if (!IsLeadershipReady() || !IsCurrentIdentity(Plan.Identity) || Plan.Phase == EDemoSquadPhase::Planning ||
	    Plan.Phase == EDemoSquadPhase::None || Plan.Phase == EDemoSquadPhase::Failed ||
	    Plan.Phase == EDemoSquadPhase::Completed)
	{
		return;
	}
	if (UsesMemberOrderPolicy())
	{
		const double Now = GetWorld()->GetTimeSeconds();
		if (Now >= NextPolicyRefreshAt)
		{
			NextPolicyRefreshAt = Now + 0.5;
			for (const auto& Member : Members)
			{
				if (!Member.bAlive || !Member.bMobile || Member.bPlayerControlled ||
				    Plan.Assignments.ContainsByPredicate(
				        [&Member](const auto& A)
				        { return A.UnitId == Member.UnitId && A.BindingGeneration == Member.BindingGeneration; }))
					continue;
				FDemoSquadAssignment Added;
				Added.UnitId = Member.UnitId;
				Added.BindingGeneration = Member.BindingGeneration;
				Added.bGuaranteed = true;
				Added.Order.UnitId = Member.UnitId;
				Added.Order.SquadId = SquadId;
				Added.Order.Identity = Plan.Identity;
				Added.Order.OrderId = FGuid::NewGuid();
				Added.Order.Engagement = Mission.Engagement;
				Plan.Assignments.Add(Added);
			}
			Plan.Assignments.RemoveAll(
			    [this](const auto& A)
			    {
				    const auto* Member = FindMember(A.UnitId);
				    if (Member && Member->bAlive && Member->BindingGeneration == A.BindingGeneration)
					    return false;
				    if (auto* Service = GetWorld()->GetSubsystem<UDemoSquadPlanningSubsystem>())
					    Service->ReleasePosition(A.PositionId, Plan.Identity.PlanId);
				    PolicyGoals.Remove(A.UnitId);
				    PolicyRetryAt.Remove(A.UnitId);
				    return true;
			    });
			for (auto& Assignment : Plan.Assignments)
				UpdatePolicyOrder(Assignment);
			PublishOrders(true);
		}
		return;
	}
	if (((Mission.Type == EDemoSquadMissionType::Defend && Plan.Phase == EDemoSquadPhase::Maintain) ||
	     (Mission.Type == EDemoSquadMissionType::Control &&
	      MissionResult.State == EDemoSquadMissionState::Succeeded)) &&
	    PhaseSatisfied())
		Plan.PhaseDeadline = 0.0;
	if (!RequiredMembersAvailable())
	{
		const bool bPlayerChanged = Plan.Assignments.ContainsByPredicate(
		    [this](const auto& Assignment)
		    {
			    const auto* Member = FindMember(Assignment.UnitId);
			    return Assignment.bGuaranteed && Assignment.bRequired && Member && Member->bPlayerControlled;
		    });
		FailPlan(bPlayerChanged ? EDemoSquadFailure::PlayerControlled : EDemoSquadFailure::Incapacitated);
		return;
	}
	UDemoSquadPlanningSubsystem* Service = GetWorld()->GetSubsystem<UDemoSquadPlanningSubsystem>();
	const double Now = GetWorld()->GetTimeSeconds();
	for (auto& Assignment : Plan.Assignments)
	{
		const auto* Member = FindMember(Assignment.UnitId);
		if (Service && Member && Member->bAlive)
		{
			Service->RenewPosition(Assignment.PositionId, Plan.Identity.PlanId);
		}
		if (!Member || !Assignment.bGuaranteed)
		{
			continue;
		}
		if (Member->bPlayerControlled)
		{
			FailPlan(EDemoSquadFailure::PlayerControlled);
			return;
		}
		if (Now - Member->ReportedAt > GetTactics().FeedbackTimeoutSeconds)
		{
			FailPlan(EDemoSquadFailure::FeedbackTimeout);
			return;
		}
		const auto& Feedback = Member->Feedback;
		if (Feedback.OrderId == Assignment.Order.OrderId && Feedback.Revision == Assignment.Order.Revision &&
		    Feedback.State == EDemoSquadOrderState::Failed && Member->bAlive && Member->bMobile)
		{
			if (Assignment.RetryCount >= 2)
			{
				FailPlan(Feedback.Failure);
				return;
			}
			++Assignment.RetryCount;
			if (!AssignPosition(Assignment, Assignment.Order.Goal.Center))
			{
				FailPlan(EDemoSquadFailure::PositionUnavailable);
				return;
			}
			++Assignment.Order.Revision;
			PublishOrders(true);
			break;
		}
	}
	if (!bPhaseDispatched)
	{
		PublishOrders(true);
	}
	if (PhaseSettings.bFollowRoute)
	{
		const int32 Before = Plan.RouteIndex;
		UpdateFormation();
		if (Before != Plan.RouteIndex || Plan.Phase == EDemoSquadPhase::Failed)
		{
			return;
		}
	}
	if (Plan.PhaseDeadline > 0.0 && Now >= Plan.PhaseDeadline && !IsPhaseReady())
	{
		FailPlan(EDemoSquadFailure::PhaseTimeout);
	}
}

bool UDemoSquadContextComponent::IsRosterReady() const
{
	return PhaseSatisfied();
}

bool UDemoSquadContextComponent::IsPhaseReady() const
{
	return PhaseSettings.Readiness != EDemoSquadReadiness::Continuous &&
	       (!PhaseSettings.bFollowRoute || Plan.RouteIndex + 1 >= Plan.Route.Num()) && PhaseSatisfied();
}

void UDemoSquadContextComponent::CompletePlan()
{
	if (IsCurrentIdentity(Plan.Identity))
	{
		SetPhase(EDemoSquadPhase::Completed);
	}
}

void UDemoSquadContextComponent::ReportMissionFailure(EDemoSquadFailure Failure)
{
	if (MissionResult.State == EDemoSquadMissionState::Running)
	{
		bMissionCompleted = true;
		SetMissionOutcome(EDemoSquadMissionState::Failed, Failure);
		CancelPlan();
		Wake();
	}
}

void UDemoSquadContextComponent::DegradeOrders()
{
	CancelPlan(true);
	for (const auto& Member : Members)
	{
		if (auto* Receiver = Nelaric::Squad::Receiver(&Member))
		{
			Receiver->EnterDegradedHold();
		}
	}
}

void UDemoSquadContextComponent::Wake()
{
	if (!bEnding && GetOwner()->HasAuthority() && ExecutionDriver.IsValid() &&
	    (GetWorld()->GetTimerManager().GetTimerRemaining(UpdateTimer) <= 0.0f ||
	     GetWorld()->GetTimerManager().GetTimerRemaining(UpdateTimer) > 0.01f))
	{
		GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::Update, 0.01f, false);
	}
}

void UDemoSquadContextComponent::Update()
{
	if (bEnding || bUpdating || !ExecutionDriver.IsValid() || GetWorld()->bIsTearingDown)
	{
		return;
	}
	UpdateTimer.Invalidate();
	TGuardValue<bool> Guard(bUpdating, true);
	RebuildSnapshot();
	// Snapshot refresh only. Tasks maintain their active phase; all tactics,
	// deadlines, outcomes and next-stage choices are authored in the tree.
	if (!GetWorld()->GetTimerManager().IsTimerActive(ChangedTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(ChangedTimer, this, &ThisClass::CommitChanged, 0.001f, false);
	}
	GetWorld()->GetTimerManager().SetTimer(UpdateTimer, this, &ThisClass::Update, 0.5f, false);
}

void UDemoSquadContextComponent::CommitChanged()
{
	ChangedTimer.Invalidate();
	if (bEnding)
	{
		return;
	}
	if (UStateTreeComponent* Tree = Cast<UStateTreeComponent>(ExecutionDriver.Get()))
	{
		Tree->SendStateTreeEvent(Nelaric::Squad::DecisionChanged);
	}
	Changed.Broadcast();
}

void UDemoSquadContextComponent::CleanupSquad()
{
	CancelPlan();
}

void UDemoSquadContextComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnding = true;
	MemberOrderHandler.Unbind();
	PolicyGoals.Reset();
	PolicyRetryAt.Reset();
	StopExecution(nullptr);
	GetWorld()->GetTimerManager().ClearTimer(ChangedTimer);
	Changed.Clear();
	TArray<TWeakObjectPtr<UDemoSquadMemberComponent>> Registered;
	MemberComponents.GenerateValueArray(Registered);
	for (auto& Entry : Registered)
	{
		if (auto* Member = Entry.Get())
		{
			Member->LeaveSquad();
		}
	}
	MemberComponents.Reset();
	Members.Reset();
	Super::EndPlay(EndPlayReason);
}
