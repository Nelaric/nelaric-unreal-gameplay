// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoObjectiveWorldSubsystem.h"
#include "AI/DemoSquadCommandActor.h"
#include "AI/DemoSquadContextComponent.h"
#include "AI/DemoSquadMemberComponent.h"
#include "AI/DemoCompanyCommandActor.h"
#include "AI/DemoCompanyRegistrySubsystem.h"
#include "Character/DemoCharacter.h"
#include "Components/BoxComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Serialization/JsonSerializer.h"

namespace Nelaric::Command
{
static FString Encode(const TSharedRef<FJsonObject>& Object)
{
	FString Result;
	FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Result));
	return Result;
}
static TSharedRef<FJsonObject> Point(FVector Location)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetNumberField(TEXT("x"), Location.X);
	Result->SetNumberField(TEXT("y"), Location.Y);
	Result->SetNumberField(TEXT("z"), Location.Z);
	return Result;
}
} // namespace Nelaric::Command

ADemoCommandArea::ADemoCommandArea()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);
	Sensor = CreateDefaultSubobject<UBoxComponent>(TEXT("RuleSensor"));
	SetRootComponent(Sensor);
	Sensor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sensor->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sensor->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Sensor->SetGenerateOverlapEvents(true);
}

void ADemoCommandArea::BeginPlay()
{
	Super::BeginPlay();
	Area.Center = GetActorLocation();
	if (!HasAuthority() || !FMath::IsFinite(Area.Radius) || Area.Radius < 1 || !FMath::IsFinite(HalfHeight) ||
	    HalfHeight < 1 || Capacity < 1)
		return;
	Sensor->SetBoxExtent(FVector(Area.Radius, Area.Radius, HalfHeight));
	Sensor->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);
	Sensor->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);
	if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
		Rules->RegisterArea(this);
	RefreshOccupants();
}

void ADemoCommandArea::RefreshOccupants()
{
	// A spatial overlap query, never a world-wide soldier scan.
	TArray<AActor*> Overlapping;
	Sensor->GetOverlappingActors(Overlapping, ADemoCharacter::StaticClass());
	Occupants.Reset();
	for (AActor* Actor : Overlapping)
		if (auto* Soldier = Cast<ADemoCharacter>(Actor))
			Occupants.AddUnique(Soldier);
}

void ADemoCommandArea::HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other,
                                     UPrimitiveComponent* OtherComponent, int32 BodyIndex, bool bSweep,
                                     const FHitResult& Hit)
{
	if (auto* Soldier = Cast<ADemoCharacter>(Other))
	{
		Occupants.AddUnique(Soldier);
		if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
			Rules->NotifyFactsChanged();
	}
}

void ADemoCommandArea::HandleEndOverlap(UPrimitiveComponent* Overlapped, AActor* Other,
                                        UPrimitiveComponent* OtherComponent, int32 BodyIndex)
{
	if (auto* Soldier = Cast<ADemoCharacter>(Other))
		if (!Sensor->IsOverlappingActor(Soldier))
		{
			Occupants.Remove(Soldier);
			if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
				Rules->NotifyFactsChanged();
		}
}

const TArray<TWeakObjectPtr<ADemoCharacter>>& ADemoCommandArea::GetOccupants() const
{
	return Occupants;
}
const TSet<FString>& ADemoCommandArea::GetSearchScopes(uint8 TeamId) const
{
	static const TSet<FString> Empty;
	const auto* Values = SearchScopes.Find(TeamId);
	return Values ? *Values : Empty;
}
int32 ADemoCommandArea::GetSearchCount(uint8 TeamId, const FString& Scope) const
{
	const auto* Team = SearchPlatoons.Find(TeamId);
	const auto* Platoons = Team ? Team->Find(Scope) : nullptr;
	return Platoons ? Platoons->Num() : 0;
}

bool ADemoCommandArea::RecordSearch(ADemoSquadCommandActor* Squad, FGuid MissionId, int32 Revision)
{
	if (!HasAuthority() || !IsValid(Squad) || Squad->GetWorld() != GetWorld())
		return false;
	const auto* Context = Squad->GetSquadContext();
	const auto Mission = Context->GetMission();
	const auto Report = Context->GetSituationReport();
	if (Mission.MissionId != MissionId || Mission.Revision != Revision ||
	    Mission.Type != EDemoSquadMissionType::Search ||
	    Report.MissionResult.State != EDemoSquadMissionState::Succeeded || Report.TeamId == 255 ||
	    FVector::DistSquared2D(Mission.Goal.Center, Area.Center) > FMath::Square(Area.Radius) ||
	    Report.ObservedAt < 0 || GetWorld()->GetTimeSeconds() - Report.ObservedAt > 3.0)
		return false;
	const AActor* Platoon = Context->GetMissionSource();
	const auto* Membership = Platoon ? Platoon->FindComponentByClass<UDemoCompanyMembershipComponent>() : nullptr;
	if (!Membership || Membership->GetSquads().IsEmpty() || Mission.ObjectiveScope.IsEmpty())
		return false;
	for (const auto& Binding : Membership->GetSquads())
	{
		const auto* Required = Binding.Get();
		if (!Required)
			return false;
		const auto* RequiredContext = Required->GetSquadContext();
		const auto RequiredMission = RequiredContext->GetMission();
		const auto RequiredReport = RequiredContext->GetSituationReport();
		if (RequiredContext->GetMissionSource() != Platoon || RequiredMission.Type != EDemoSquadMissionType::Search ||
		    RequiredMission.ObjectiveScope != Mission.ObjectiveScope ||
		    RequiredReport.MissionResult.State != EDemoSquadMissionState::Succeeded || RequiredReport.ObservedAt < 0 ||
		    GetWorld()->GetTimeSeconds() - RequiredReport.ObservedAt > 3.0)
			return false;
	}
	auto& Scopes = SearchScopes.FindOrAdd(Report.TeamId);
	if (Scopes.Num() >= 128 && !Scopes.Contains(Mission.ObjectiveScope))
		return false;
	Scopes.Add(Mission.ObjectiveScope);
	SearchPlatoons.FindOrAdd(Report.TeamId).FindOrAdd(Mission.ObjectiveScope).Add(Platoon->GetPathName());
	return true;
}

void ADemoCommandArea::EndPlay(const EEndPlayReason::Type Reason)
{
	if (auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
		Rules->UnregisterArea(this);
	Sensor->OnComponentBeginOverlap.RemoveAll(this);
	Sensor->OnComponentEndOverlap.RemoveAll(this);
	Super::EndPlay(Reason);
}

bool UDemoObjectiveWorldSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
	return Type == EWorldType::Game || Type == EWorldType::PIE;
}
void UDemoObjectiveWorldSubsystem::NotifyFactsChanged()
{
	const auto* Registry = GetWorld()->GetSubsystem<UDemoCompanyRegistrySubsystem>();
	if (Registry)
		for (AActor* Publisher : Registry->GetCompanies())
			if (auto* Actor = Cast<ADemoCompanyCommandActor>(Publisher))
				if (Actor->GetCompanyContext()->InputRevision < MAX_int32)
					++Actor->GetCompanyContext()->InputRevision;
}

bool UDemoObjectiveWorldSubsystem::RegisterArea(ADemoCommandArea* Area)
{
	if (!IsValid(Area) || !Area->HasAuthority() || Area->GetWorld() != GetWorld() || Area->AreaId.IsEmpty())
		return false;
	if (const auto* Existing = Areas.Find(Area->AreaId))
		if (Existing->IsValid() && Existing->Get() != Area)
			return false;
	Areas.Add(Area->AreaId, Area);
	return true;
}

void UDemoObjectiveWorldSubsystem::UnregisterArea(ADemoCommandArea* Area)
{
	if (Area && Areas.FindRef(Area->AreaId).Get() == Area)
		Areas.Remove(Area->AreaId);
}

ADemoCommandArea* UDemoObjectiveWorldSubsystem::FindArea(const FString& AreaId) const
{
	return Areas.FindRef(AreaId).Get();
}

void UDemoObjectiveWorldSubsystem::RegisterSoldier(FGuid UnitId, ADemoCharacter* Soldier)
{
	if (!CanBindSoldier(UnitId, Soldier))
		return;
	const FString Key = UnitId.ToString(EGuidFormats::Digits).ToUpper();
	Soldiers.Add(Key, Soldier);
}
bool UDemoObjectiveWorldSubsystem::CanBindSoldier(FGuid UnitId, ADemoCharacter* Soldier) const
{
	if (!UnitId.IsValid() || !IsValid(Soldier) || !Soldier->HasAuthority() || Soldier->GetWorld() != GetWorld())
		return false;
	const auto Existing = Soldiers.FindRef(UnitId.ToString(EGuidFormats::Digits).ToUpper());
	return !Existing.IsValid() || Existing.Get() == Soldier;
}
void UDemoObjectiveWorldSubsystem::RecordSoldierDeath(FGuid UnitId)
{
	if (UnitId.IsValid() && !CommittedDeaths.Contains(UnitId.ToString(EGuidFormats::Digits).ToUpper()))
	{
		CommittedDeaths.Add(UnitId.ToString(EGuidFormats::Digits).ToUpper());
		NotifyFactsChanged();
	}
}

void UDemoObjectiveWorldSubsystem::UnregisterSoldier(FGuid UnitId, ADemoCharacter* Soldier)
{
	const FString Key = UnitId.ToString(EGuidFormats::Digits).ToUpper();
	if (Soldiers.FindRef(Key).Get() != Soldier)
		return;
	if (Soldier && Soldier->HasCommittedDeath())
		CommittedDeaths.Add(Key);
	Soldiers.Remove(Key);
}

FString UDemoObjectiveWorldSubsystem::GetAreaDefinitions() const
{
	auto Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Definitions;
	for (const auto& Entry : Areas)
		if (const auto* Area = Entry.Value.Get())
		{
			auto Definition = MakeShared<FJsonObject>();
			Definition->SetStringField(TEXT("id"), Entry.Key);
			Definition->SetNumberField(TEXT("capacity"), Area->Capacity);
			auto Goal = MakeShared<FJsonObject>();
			Goal->SetObjectField(TEXT("center"), Nelaric::Command::Point(Area->Area.Center));
			Goal->SetNumberField(TEXT("radius"), Area->Area.Radius);
			Definition->SetObjectField(TEXT("goal"), Goal);
			TArray<TSharedPtr<FJsonValue>> Passages;
			for (const auto& Passage : Area->Passages)
			{
				auto Value = MakeShared<FJsonObject>();
				Value->SetStringField(TEXT("id"), Passage.Id);
				Value->SetStringField(TEXT("to"), Passage.To);
				Value->SetNumberField(TEXT("capacity"), Passage.Capacity);
				Value->SetBoolField(TEXT("available"), Passage.bAvailable);
				Value->SetNumberField(TEXT("opensAt"), Passage.OpensAt);
				Value->SetNumberField(TEXT("closesAt"), Passage.ClosesAt);
				Passages.Add(MakeShared<FJsonValueObject>(Value));
			}
			Definition->SetArrayField(TEXT("passages"), Passages);
			Definitions.Add(MakeShared<FJsonValueObject>(Definition));
		}
	Root->SetArrayField(TEXT("areas"), Definitions);
	return Nelaric::Command::Encode(Root);
}

FString UDemoObjectiveWorldSubsystem::GetRuleInputs(uint8 TeamId, const TArray<FString>& Participants)
{
	check(IsInGameThread());
	auto Root = MakeShared<FJsonObject>();
	auto Samples = MakeShared<FJsonObject>();
	auto Units = MakeShared<FJsonObject>();
	const double Now = GetWorld()->GetTimeSeconds();
	if (GetWorld()->GetNetMode() == NM_Client || TeamId == 255)
		return TEXT("{\"areas\":{},\"units\":{}}");
	for (const auto& Entry : Areas)
		if (auto* Area = Entry.Value.Get())
		{
			auto Sample = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> Present;
			bool bContested = false;
			bool bKnown = true;
			TArray<FVector> Locations;
			for (const auto& Binding : Area->GetOccupants())
				if (auto* Soldier = Binding.Get())
				{
					if (!Soldier->IsPoolActive() || !Soldier->IsAlive() || Soldier->HasCommittedDeath() ||
					    Soldier->IsActorBeingDestroyed() ||
					    FVector::DistSquared2D(Soldier->GetActorLocation(), Area->Area.Center) >
					        FMath::Square(Area->Area.Radius))
						continue;
					if (Soldier->GetTeamId() == 255)
					{
						bKnown = false;
						continue;
					}
					if (Soldier->GetTeamId() != TeamId)
					{
						bContested = true;
						continue;
					}
					const auto* Member = Soldier->FindComponentByClass<UDemoSquadMemberComponent>();
					if (Member && Member->GetUnitId().IsValid())
					{
						Present.Add(
						    MakeShared<FJsonValueString>(Member->GetUnitId().ToString(EGuidFormats::Digits).ToUpper()));
						Locations.Add(Soldier->GetActorLocation());
					}
				}
			Sample->SetBoolField(TEXT("known"), bKnown);
			Sample->SetNumberField(TEXT("observedAt"), Now);
			Sample->SetArrayField(TEXT("presentIds"), Present);
			Sample->SetBoolField(TEXT("contested"), bContested);
			FVector Center = FVector::ZeroVector;
			for (const auto& Location : Locations)
				Center += Location;
			if (!Locations.IsEmpty())
				Center /= Locations.Num();
			const bool bGathered =
			    !Locations.IsEmpty() &&
			    !Locations.ContainsByPredicate(
			        [Center, Area](const auto& Location)
			        { return FVector::DistSquared2D(Location, Center) > FMath::Square(Area->Area.Radius * 0.5f); });
			Sample->SetBoolField(TEXT("gathered"), bGathered);
			Sample->SetBoolField(TEXT("searched"), false);
			TArray<TSharedPtr<FJsonValue>> SearchScopes;
			auto SearchCounts = MakeShared<FJsonObject>();
			for (const auto& Scope : Area->GetSearchScopes(TeamId))
			{
				SearchScopes.Add(MakeShared<FJsonValueString>(Scope));
				SearchCounts->SetNumberField(Scope, Area->GetSearchCount(TeamId, Scope));
			}
			Sample->SetArrayField(TEXT("searchScopes"), SearchScopes);
			Sample->SetObjectField(TEXT("searchCounts"), SearchCounts);
			Samples->SetObjectField(Entry.Key, Sample);
		}
	for (const auto& Id : Participants)
	{
		auto Unit = MakeShared<FJsonObject>();
		auto* Soldier = Soldiers.FindRef(Id.Replace(TEXT("-"), TEXT("")).ToUpper()).Get();
		const bool bDead = CommittedDeaths.Contains(Id.Replace(TEXT("-"), TEXT("")).ToUpper());
		const bool bKnown = (Soldier && Soldier->IsPoolActive()) || bDead;
		Unit->SetBoolField(TEXT("known"), bKnown);
		Unit->SetBoolField(TEXT("alive"),
		                   Soldier && Soldier->IsPoolActive() && !Soldier->HasCommittedDeath() && !bDead);
		Unit->SetNumberField(TEXT("teamId"), Soldier ? Soldier->GetTeamId() : TeamId);
		Unit->SetObjectField(TEXT("location"), Nelaric::Command::Point(Soldier && Soldier->GetTeamId() == TeamId
		                                                                   ? Soldier->GetActorLocation()
		                                                                   : FVector::ZeroVector));
		Unit->SetNumberField(TEXT("observedAt"), bKnown ? Now : -1);
		Units->SetObjectField(Id, Unit);
	}
	Root->SetObjectField(TEXT("areas"), Samples);
	Root->SetObjectField(TEXT("units"), Units);
	return Nelaric::Command::Encode(Root);
}

TArray<FString> UDemoObjectiveWorldSubsystem::GetPlatoonParticipants(AActor* Platoon, uint8 TeamId) const
{
	TArray<FString> Result;
	if (!IsValid(Platoon) || !Platoon->HasAuthority() || Platoon->GetWorld() != GetWorld())
		return Result;
	const auto* Membership = Platoon->FindComponentByClass<UDemoCompanyMembershipComponent>();
	if (!Membership || !Membership->GetCompanySource())
		return Result;
	for (const auto& WeakSquad : Membership->GetSquads())
		if (const auto* Squad = WeakSquad.Get())
			if (Squad->GetSquadContext()->GetSituationReport().TeamId == TeamId)
				for (const auto& Member : Squad->GetSquadContext()->GetMembers())
					Result.AddUnique(Member.UnitId.ToString(EGuidFormats::Digits).ToUpper());
	Result.Sort();
	return Result;
}

void UDemoObjectiveWorldSubsystem::Deinitialize()
{
	Areas.Reset();
	Soldiers.Reset();
	CommittedDeaths.Reset();
	Super::Deinitialize();
}
