// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoCompanyCommandActor.h"
#include "AI/DemoCompanyRegistrySubsystem.h"
#include "AI/DemoSquadCommandActor.h"
#include "AI/DemoSquadContextComponent.h"
#include "Components/StateTreeComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "Serialization/JsonSerializer.h"
#include "StateTreeExecutionContext.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace Nelaric::Command
{
static TSharedPtr<FJsonObject> Parse(const FString& Json)
{
	TSharedPtr<FJsonObject> Value;
	if (Json.Len() > 1048576 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Value))
		return nullptr;
	return Value;
}
} // namespace Nelaric::Command

UDemoCompanyContextComponent::UDemoCompanyContextComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	NextPhase = TEXT("Bootstrap");
	CommandMode = TEXT("Autonomous");
}
FString UDemoCompanyContextComponent::GetState() const
{
	return State;
}
bool UDemoCompanyContextComponent::PublishState(const FString& Snapshot)
{
	check(IsInGameThread());
	if (!GetWorld() || !GetOwner())
		return false;
	const auto* Registry = GetWorld()->GetSubsystem<UDemoCompanyRegistrySubsystem>();
	const auto Value = Nelaric::Command::Parse(Snapshot);
	if (!GetOwner()->HasAuthority() || !Registry || !Registry->IsRegisteredCompany(GetOwner()) || !Value)
		return false;
	double Epoch = 0;
	FString Phase;
	if (!Value->TryGetNumberField(TEXT("commandEpoch"), Epoch) || Epoch < CommandEpoch || Epoch > MAX_int32 ||
	    !Value->TryGetStringField(TEXT("nextPhase"), Phase) || Phase.Len() > 64)
		return false;
	FString Mode;
	if (!Value->TryGetStringField(TEXT("mode"), Mode))
		return false;
	CommandEpoch = int32(Epoch);
	NextPhase = FName(Phase);
	CommandMode = FName(Mode);
	State = Snapshot;
	return true;
}

ADemoCompanyCommandActor::ADemoCompanyCommandActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
	Context = CreateDefaultSubobject<UDemoCompanyContextComponent>(TEXT("CompanyContext"));
	Tree = CreateDefaultSubobject<UStateTreeComponent>(TEXT("CompanyStateTree"));
	Tree->SetStartLogicAutomatically(false);
}
UDemoCompanyContextComponent* ADemoCompanyCommandActor::GetCompanyContext() const
{
	return Context;
}
UStateTreeComponent* ADemoCompanyCommandActor::GetCommandTree() const
{
	return Tree;
}
bool ADemoCompanyCommandActor::StartCommander_Implementation()
{
	return false;
}
void ADemoCompanyCommandActor::StopCommander_Implementation()
{
	Tree->StopLogic(TEXT("Company stopped"));
}
bool ADemoCompanyCommandActor::SubmitCompanyMission_Implementation(const FDemoCompanyMission& Mission)
{
	return false;
}
bool ADemoCompanyCommandActor::UpdateBattlefrontState(AActor* Publisher, const FString& Snapshot)
{
	check(IsInGameThread());
	if (!HasAuthority() || !GetWorld() || !IsValid(Publisher) || Publisher != GetWorld()->GetAuthGameMode() ||
	    Snapshot.Len() > 65536 || !BattlefrontUpdateHandler.IsBound())
		return false;
	return BattlefrontUpdateHandler.Execute(Publisher, Snapshot);
}
bool ADemoCompanyCommandActor::SetPlatoonManualScope_Implementation(const FString& PlatoonId, bool bLocked)
{
	return false;
}
bool ADemoCompanyCommandActor::SubmitManualPlatoonMission_Implementation(const FString& PlatoonId,
                                                                         const FDemoPlatoonMission& Mission)
{
	return false;
}
EStateTreeRunStatus ADemoCompanyCommandActor::StepCompany_Implementation(FName Phase)
{
	return EStateTreeRunStatus::Failed;
}
void ADemoCompanyCommandActor::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority() && bStartOnBeginPlay)
		StartCommander();
}
void ADemoCompanyCommandActor::EndPlay(const EEndPlayReason::Type Reason)
{
	StopCommander();
	Super::EndPlay(Reason);
}

const UStruct* FDemoCompanyNextPhase::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}
bool FDemoCompanyNextPhase::TestCondition(FStateTreeExecutionContext& Context) const
{
	const auto& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	return IsValid(Data.Actor) && Data.Actor->GetCompanyContext()->NextPhase == Data.Phase;
}

FDemoCompanyWorkflowTask::FDemoCompanyWorkflowTask()
{
	bShouldCallTick = true;
}
const UStruct* FDemoCompanyWorkflowTask::GetInstanceDataType() const
{
	return FInstanceDataType::StaticStruct();
}
EStateTreeRunStatus FDemoCompanyWorkflowTask::EnterState(FStateTreeExecutionContext& Context,
                                                         const FStateTreeTransitionResult& Transition) const
{
	return Tick(Context, 0.0f);
}
EStateTreeRunStatus FDemoCompanyWorkflowTask::Tick(FStateTreeExecutionContext& Context, float DeltaTime) const
{
	const auto& Data = Context.GetInstanceData<FInstanceDataType>(*this);
	TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("Company.%s"), *Data.Operation.ToString()));
	if (!IsValid(Data.Actor) || !Data.Actor->HasAuthority())
		return EStateTreeRunStatus::Failed;
	const double Started = FPlatformTime::Seconds();
	const EStateTreeRunStatus Result = Data.Actor->StepCompany(Data.Operation);
	const double Duration = FPlatformTime::Seconds() - Started;
	auto* Company = Data.Actor->GetCompanyContext();
	++Company->WorkflowCalls;
	Company->WorkflowSeconds += Duration;
	Company->MaximumWorkflowSeconds = FMath::Max(Company->MaximumWorkflowSeconds, Duration);
	return Result;
}

EStateTreeRunStatus UDemoCompanyMembershipComponent::StepPlatoon(const FString& Operation)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !StepHandler.IsBound())
		return EStateTreeRunStatus::Failed;
	return StepHandler.Execute(Operation);
}
UDemoCompanyMembershipComponent::UDemoCompanyMembershipComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}
bool UDemoCompanyMembershipComponent::ValidSource(AActor* Source) const
{
	if (!GetWorld() || !GetOwner())
		return false;
	const auto* Registry = GetWorld()->GetSubsystem<UDemoCompanyRegistrySubsystem>();
	return !GetWorld()->bIsTearingDown && GetOwner()->HasAuthority() && IsValid(Source) && Source->HasAuthority() &&
	       Source->GetWorld() == GetWorld() && Registry && Registry->IsRegisteredCompany(Source) &&
	       Company.Get() == Source;
}
int32 UDemoCompanyMembershipComponent::ClaimCompany(AActor* Source, const FString& PlatoonId)
{
	check(IsInGameThread());
	const auto* Registry = GetWorld()->GetSubsystem<UDemoCompanyRegistrySubsystem>();
	if (!Registry || !Registry->IsRegisteredCompany(Source) || !IsValid(Source) || !Source->HasAuthority() ||
	    !GetOwner()->HasAuthority() || Source->GetWorld() != GetWorld() || PlatoonId.IsEmpty() ||
	    (Company.IsValid() && Company.Get() != Source) || MembershipRevision == MAX_int32)
		return 0;
	const auto* Command = Cast<ADemoCompanyCommandActor>(Source);
	if (!Command || !Command->Definition || !Command->Definition->ExpectedPlatoonIds.Contains(PlatoonId) ||
	    Squads.IsEmpty())
		return 0;
	for (const auto& Squad : Squads)
		if (!Squad.IsValid() || Squad->GetSquadContext()->GetSituationReport().TeamId != Command->Definition->TeamId)
			return 0;
	if (Company.Get() == Source)
		return Identity == PlatoonId ? MembershipRevision : 0;
	Company = Source;
	Identity = PlatoonId;
	Candidate.Reset();
	ActiveId.Reset();
	ActiveRevision = 0;
	return ++MembershipRevision;
}
bool UDemoCompanyMembershipComponent::ReleaseCompany(AActor* Source, int32 Revision)
{
	if (Company.Get() != Source || Revision != MembershipRevision)
		return false;
	Company.Reset();
	Candidate.Reset();
	ActiveId.Reset();
	ActiveRevision = 0;
	bManualScope = false;
	return true;
}
bool UDemoCompanyMembershipComponent::StageAssignment(AActor* Source, const FString& Assignment, bool bPlayerAuthorized)
{
	check(IsInGameThread());
	const auto Value = Nelaric::Command::Parse(Assignment);
	const auto* Actor = Cast<ADemoCompanyCommandActor>(Source);
	const auto* Registry = GetWorld()->GetSubsystem<UDemoCompanyRegistrySubsystem>();
	if (!ValidSource(Source) || !Actor || !Value || (bManualScope && !bPlayerAuthorized) || !Actor->Definition ||
	    Actor->GetCompanyContext()->CommandMode == TEXT("Suspended") ||
	    (!bPlayerAuthorized && Actor->GetCompanyContext()->CommandMode == TEXT("PlayerManual")))
		return false;
	FString Id, Recipient, RunId, CompanyId;
	double Revision = 0, Membership = 0, Epoch = 0;
	if (!Value->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty() ||
	    !Value->TryGetStringField(TEXT("platoonId"), Recipient) || Recipient != Identity ||
	    !Value->TryGetStringField(TEXT("runId"), RunId) || RunId != Registry->GetRunId() ||
	    !Value->TryGetStringField(TEXT("companyId"), CompanyId) || CompanyId != Actor->Definition->CompanyId ||
	    !Value->TryGetNumberField(TEXT("membershipRevision"), Membership) || Membership != MembershipRevision ||
	    !Value->TryGetNumberField(TEXT("commandEpoch"), Epoch) || Epoch != Actor->GetCompanyContext()->CommandEpoch ||
	    !Value->TryGetNumberField(TEXT("revision"), Revision) || Revision < 1 || Revision > MAX_int32 ||
	    (Id == ActiveId && Revision <= ActiveRevision))
		return false;
	if (!Candidate.IsEmpty() && Candidate != Assignment)
		return false;
	Candidate = Assignment;
	bCandidatePlayerAuthorized = bPlayerAuthorized;
	return true;
}
bool UDemoCompanyMembershipComponent::ReconcileAssignment(AActor* Source, const FString& Assignment)
{
	const auto Value = Nelaric::Command::Parse(Assignment);
	const auto* Actor = Cast<ADemoCompanyCommandActor>(Source);
	const auto* Registry = GetWorld()->GetSubsystem<UDemoCompanyRegistrySubsystem>();
	FString Id, Recipient, RunId, CompanyId;
	double Revision = 0, Epoch = 0, Membership = 0;
	if (!ValidSource(Source) || !Actor || !Actor->Definition || !Value || !Value->TryGetStringField(TEXT("id"), Id) ||
	    !Value->TryGetStringField(TEXT("platoonId"), Recipient) || Recipient != Identity ||
	    !Value->TryGetStringField(TEXT("runId"), RunId) || RunId != Registry->GetRunId() ||
	    !Value->TryGetStringField(TEXT("companyId"), CompanyId) || CompanyId != Actor->Definition->CompanyId ||
	    !Value->TryGetNumberField(TEXT("commandEpoch"), Epoch) || Epoch != Actor->GetCompanyContext()->CommandEpoch ||
	    !Value->TryGetNumberField(TEXT("membershipRevision"), Membership) || Membership != MembershipRevision ||
	    !Value->TryGetNumberField(TEXT("revision"), Revision) || Id.IsEmpty() || Revision < 1 || Revision > MAX_int32 ||
	    (!ActiveId.IsEmpty() && (ActiveId != Id || ActiveRevision != Revision)))
		return false;
	ActiveId = Id;
	ActiveRevision = int32(Revision);
	Candidate.Reset();
	return true;
}
bool UDemoCompanyMembershipComponent::ActivateAssignment(AActor* Source, const FDemoExecutionPermit& Permit)
{
	check(IsInGameThread());
	const auto* Actor = Cast<ADemoCompanyCommandActor>(Source);
	const auto* Registry = GetWorld()->GetSubsystem<UDemoCompanyRegistrySubsystem>();
	if (!ValidSource(Source) || !Actor || (bManualScope && !bCandidatePlayerAuthorized) || !Permit.bAllowed ||
	    Permit.GateVersion < 1 || Permit.PhaseId != TEXT("Execute") ||
	    Permit.MembershipRevision != MembershipRevision ||
	    Permit.CommandEpoch != Actor->GetCompanyContext()->CommandEpoch || Permit.RunId != Registry->GetRunId() ||
	    Permit.CompanyId != Actor->Definition->CompanyId)
		return false;
	if (ActiveId == Permit.AssignmentId && ActiveRevision == Permit.AssignmentRevision)
		return true;
	const auto Value = Nelaric::Command::Parse(Candidate);
	FString Id;
	double Revision = 0;
	if (!Value || !Value->TryGetStringField(TEXT("id"), Id) || Id != Permit.AssignmentId ||
	    !Value->TryGetNumberField(TEXT("revision"), Revision) || Revision != Permit.AssignmentRevision)
		return false;
	ActiveId = Id;
	ActiveRevision = int32(Revision);
	Candidate.Reset();
	return true;
}
bool UDemoCompanyMembershipComponent::CancelAssignment(AActor* Source, const FString& Id, int32 Revision)
{
	// Exact owned cancellation remains valid during authority/world teardown.
	if (!Source || Company.Get() != Source || !GetOwner()->HasAuthority() || Source->GetWorld() != GetWorld())
		return false;
	bool bCancelled = false;
	if (ActiveId == Id && ActiveRevision == Revision)
	{
		ActiveId.Reset();
		ActiveRevision = 0;
		bCancelled = true;
	}
	const auto Value = Nelaric::Command::Parse(Candidate);
	FString CandidateId;
	double CandidateRevision = 0;
	if (Value && Value->TryGetStringField(TEXT("id"), CandidateId) && CandidateId == Id &&
	    Value->TryGetNumberField(TEXT("revision"), CandidateRevision) && CandidateRevision == Revision)
	{
		Candidate.Reset();
		bCancelled = true;
	}
	return bCancelled;
}
bool UDemoCompanyMembershipComponent::SetManualScope(AActor* Source, bool bLocked)
{
	if (!ValidSource(Source))
		return false;
	bManualScope = bLocked;
	if (bLocked)
		Candidate.Reset();
	return true;
}
int32 UDemoCompanyMembershipComponent::GetMembershipRevision() const
{
	return MembershipRevision;
}
AActor* UDemoCompanyMembershipComponent::GetCompanySource() const
{
	return Company.Get();
}

bool UDemoCompanyMembershipComponent::WatchSquad(ADemoSquadCommandActor* Squad)
{
	if (!IsValid(Squad) || Squad->GetWorld() != GetWorld() || !GetOwner()->HasAuthority() ||
	    Squad->GetSquadContext()->GetMissionSource() != GetOwner())
		return false;
	if (Squads.Contains(Squad))
		return true;
	Squads.Add(Squad);
	SquadHandles.Add(Squad->GetSquadContext()->OnChanged().AddUObject(this, &ThisClass::HandleSquadChanged,
	                                                                  TWeakObjectPtr<ADemoSquadCommandActor>(Squad)));
	return true;
}
const TArray<TWeakObjectPtr<ADemoSquadCommandActor>>& UDemoCompanyMembershipComponent::GetSquads() const
{
	return Squads;
}
void UDemoCompanyMembershipComponent::HandleSquadChanged(TWeakObjectPtr<ADemoSquadCommandActor> Squad)
{
	if (!Squad.IsValid())
		return;
	const auto Report = Squad->GetSquadContext()->GetSituationReport();
	uint32 Signature = GetTypeHash(Report.MissionResult.MissionId);
	Signature = HashCombine(Signature, GetTypeHash(Report.MissionResult.Revision));
	Signature = HashCombine(Signature, GetTypeHash(uint8(Report.MissionResult.State)));
	Signature = HashCombine(Signature, GetTypeHash(Report.bReady));
	Signature = HashCombine(Signature, GetTypeHash(Report.bCommandAvailable));
	Signature = HashCombine(Signature, GetTypeHash(Report.MembershipEpoch));
	Signature = HashCombine(Signature, GetTypeHash(Report.CommandEpoch));
	Signature = HashCombine(Signature, GetTypeHash(Report.GateVersion));
	Signature = HashCombine(Signature, GetTypeHash(Report.TeamId));
	Signature = HashCombine(Signature, GetTypeHash(Report.Capability.EffectiveMemberCount));
	Signature = HashCombine(Signature, GetTypeHash(Report.Capability.MobileMemberCount));
	Signature = HashCombine(Signature, GetTypeHash(FMath::FloorToInt(Report.Capability.AmmoReadiness * 10)));
	Signature = HashCombine(Signature, GetTypeHash(FMath::FloorToInt(Report.Capability.KnownThreatPressure * 10)));
	if (const auto* Previous = SquadSignatures.Find(Squad); Previous && *Previous == Signature)
		return;
	SquadSignatures.Add(Squad, Signature);
	if (InputRevision < MAX_int32)
		++InputRevision;
	if (auto* Actor = Cast<ADemoCompanyCommandActor>(Company.Get()))
		if (Actor->GetCompanyContext()->InputRevision < MAX_int32)
			++Actor->GetCompanyContext()->InputRevision;
}
void UDemoCompanyMembershipComponent::ReleaseSquadWatches()
{
	for (int32 Index = 0; Index < Squads.Num(); ++Index)
		if (auto* Squad = Squads[Index].Get())
			Squad->GetSquadContext()->OnChanged().Remove(SquadHandles[Index]);
	Squads.Reset();
	SquadHandles.Reset();
	SquadSignatures.Reset();
}
void UDemoCompanyMembershipComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	StepHandler.Unbind();
	ReleaseSquadWatches();
	Company.Reset();
	Super::EndPlay(Reason);
}
