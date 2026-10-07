// Copyright (c) 2026 Nelaric Contributors

#include "AI/DemoCompanyRegistrySubsystem.h"
#include "AI/DemoObjectiveWorldSubsystem.h"
#include "AI/DemoCompanyCommandActor.h"
#include "AI/DemoSquadCommandActor.h"
#include "AI/DemoSquadContextComponent.h"
#include "AI/DemoSquadMemberComponent.h"
#include "Character/DemoCharacter.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "JsonObjectConverter.h"
#include "Kismet/GameplayStatics.h"

void UDemoCompanyRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RunId = FGuid::NewGuid().ToString();
}

bool UDemoCompanyRegistrySubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

int32 UDemoCompanyRegistrySubsystem::RegisterCompany(AActor* Actor, const FString& CompanyId)
{
	check(IsInGameThread());
	if (bStopping || !IsValid(Actor) || !Actor->IsA<ADemoCompanyCommandActor>() || Actor->IsActorBeingDestroyed() ||
	    Actor->GetWorld() != GetWorld() || !Actor->HasAuthority() || GetWorld()->GetNetMode() == NM_Client ||
	    CompanyId.IsEmpty() || CompanyId.Len() > 128 || Generation == MAX_int32)
		return 0;
	if (GetCompany())
	{
		if (Company.Get() == Actor && Identity == CompanyId)
			return Generation;
		UE_LOG(LogTemp, Error, TEXT("Duplicate company command rejected in world %s."), *GetWorld()->GetName());
		return 0;
	}
	Company = Actor;
	Identity = CompanyId;
	return ++Generation;
}

bool UDemoCompanyRegistrySubsystem::UnregisterCompany(AActor* Actor, int32 Epoch)
{
	check(IsInGameThread());
	if (Company.Get() != Actor || Epoch != Generation)
		return false;
	Company.Reset();
	Identity.Reset();
	return true;
}

AActor* UDemoCompanyRegistrySubsystem::GetCompany() const
{
	return !bStopping && Company.IsValid() && !Company->IsActorBeingDestroyed() ? Company.Get() : nullptr;
}

bool UDemoCompanyRegistrySubsystem::HasPublicationAuthority(AActor* Actor, int32 Epoch) const
{
	return GetCompany() == Actor && Actor && Actor->HasAuthority() && GetWorld()->GetNetMode() != NM_Client &&
	       Epoch == Generation && Epoch > 0;
}

FString UDemoCompanyRegistrySubsystem::GetRunId() const
{
	return RunId;
}

void UDemoCompanyRegistrySubsystem::Deinitialize()
{
	bStopping = true;
	Company.Reset();
	Identity.Reset();
	Super::Deinitialize();
}

UDemoCompanyRegistrySubsystem* UDemoCommandLibrary::GetRegistry(UObject* WorldContext)
{
	check(IsInGameThread());
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UDemoCompanyRegistrySubsystem>() : nullptr;
}

FString UDemoCommandLibrary::EncodeCompanyMission(const FDemoCompanyMission& Mission)
{
	FString Result;
	FJsonObjectConverter::UStructToJsonObjectString(Mission, Result);
	return Result;
}

ADemoCompanyCommandActor*
UDemoCommandLibrary::CreateCompany(UObject* WorldContext, TSubclassOf<ADemoCompanyCommandActor> CommandClass,
                                   UDemoCompanyDefinition* Definition, UDemoCompanyPolicy* Policy,
                                   UDemoCompanyMissionAsset* Mission, const TArray<AActor*>& Platoons)
{
	check(IsInGameThread());
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	if (!World || World->GetNetMode() == NM_Client || !CommandClass || !IsValid(Definition) || !IsValid(Policy) ||
	    Definition->CompanyId.IsEmpty() || Definition->TeamId == 255 || Definition->ExpectedPlatoonIds.IsEmpty() ||
	    Definition->ExpectedPlatoonIds.Num() > 64 || Platoons.Num() != Definition->ExpectedPlatoonIds.Num())
		return nullptr;
	auto* Registry = World->GetSubsystem<UDemoCompanyRegistrySubsystem>();
	if (!Registry)
		return nullptr;
	if (auto* Existing = Cast<ADemoCompanyCommandActor>(Registry->GetCompany()))
		return Existing->Definition && Existing->Definition->CompanyId == Definition->CompanyId &&
		               Existing->Definition->TeamId == Definition->TeamId
		           ? Existing
		           : nullptr;
	TSet<AActor*> Unique;
	for (AActor* Platoon : Platoons)
	{
		if (!IsValid(Platoon) || Platoon->GetWorld() != World || !Platoon->HasAuthority() ||
		    !Platoon->FindComponentByClass<UDemoCompanyMembershipComponent>() || Unique.Contains(Platoon))
			return nullptr;
		Unique.Add(Platoon);
	}
	auto* Company = World->SpawnActorDeferred<ADemoCompanyCommandActor>(CommandClass, FTransform::Identity);
	if (!Company)
		return nullptr;
	Company->Definition = Definition;
	Company->Policy = Policy;
	Company->InitialMission = Mission;
	Company->Platoons.Reset();
	for (AActor* Platoon : Platoons)
		Company->Platoons.Add(Platoon);
	Company->bStartOnBeginPlay = false;
	Company->FinishSpawning(FTransform::Identity);
	if (!Company->StartCommander())
	{
		Company->Destroy();
		return nullptr;
	}
	return Company;
}

UDemoObjectiveWorldSubsystem* UDemoCommandLibrary::GetObjectives(UObject* WorldContext)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UDemoObjectiveWorldSubsystem>() : nullptr;
}

FString UDemoCommandLibrary::EncodePlatoonMission(const FDemoPlatoonMission& Mission)
{
	FString Result;
	FJsonObjectConverter::UStructToJsonObjectString(Mission, Result);
	return Result;
}

namespace Nelaric::CommandSave
{
static FString BodyKey(const ADemoCharacter& Character)
{
	FString Key = Character.GetPathName();
	const int32 Instance = Character.GetWorld()->GetOutermost()->GetPIEInstanceID();
	if (Instance >= 0)
		Key.ReplaceInline(*FString::Printf(TEXT("UEDPIE_%d_"), Instance), TEXT(""));
	return Key;
}
} // namespace Nelaric::CommandSave

FString UDemoCommandLibrary::CaptureSquadBindings(ADemoSquadCommandActor* Squad)
{
	if (!IsValid(Squad) || !Squad->HasAuthority())
		return {};
	auto Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Members;
	for (const auto& Member : Squad->GetSquadContext()->GetMembers())
		if (auto* Character = Member.Character.Get())
		{
			auto Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("key"), Nelaric::CommandSave::BodyKey(*Character));
			Item->SetStringField(TEXT("unitId"), Member.UnitId.ToString(EGuidFormats::Digits));
			Members.Add(MakeShared<FJsonValueObject>(Item));
		}
	Root->SetArrayField(TEXT("members"), Members);
	FString Json;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
	return Json;
}

bool UDemoCommandLibrary::RestoreSquadBindings(ADemoSquadCommandActor* Squad, const FString& Bindings, bool bApply)
{
	check(IsInGameThread());
	if (!IsValid(Squad) || !Squad->HasAuthority() || Bindings.Len() > 65536)
		return false;
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Bindings), Root) || !Root)
		return false;
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Root->TryGetArrayField(TEXT("members"), Items) || Items->Num() > 64)
		return false;
	TArray<TPair<UDemoSquadMemberComponent*, FGuid>> Restores;
	TSet<FString> Keys;
	TSet<FGuid> Identities;
	const auto Members = Squad->GetSquadContext()->GetMembers();
	for (const auto& Value : *Items)
	{
		const auto Item = Value->AsObject();
		FString Key, Id;
		FGuid Identity;
		if (!Item || !Item->TryGetStringField(TEXT("key"), Key) || !Item->TryGetStringField(TEXT("unitId"), Id) ||
		    !FGuid::Parse(Id, Identity) || !Identity.IsValid() || Keys.Contains(Key) || Identities.Contains(Identity))
			return false;
		UDemoSquadMemberComponent* Target = nullptr;
		for (const auto& Member : Members)
			if (auto* Character = Member.Character.Get())
				if (Nelaric::CommandSave::BodyKey(*Character) == Key)
					Target = Character->FindComponentByClass<UDemoSquadMemberComponent>();
		if (!Target)
			return false;
		if (const auto* Rules = Squad->GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>())
			if (!Rules->CanBindSoldier(Identity, Cast<ADemoCharacter>(Target->GetOwner())))
				return false;
		for (const auto& Member : Members)
			if (Member.UnitId == Identity && Member.Character.Get() != Target->GetOwner())
				return false;
		Keys.Add(Key);
		Identities.Add(Identity);
		Restores.Emplace(Target, Identity);
	}
	if (!bApply)
		return true;
	for (const auto& Restore : Restores)
		if (!Restore.Key->RestoreUnitIdentity(Restore.Value))
			return false;
	return true;
}

namespace Nelaric::CommandSave
{
static bool ValidSlot(const FString& Slot)
{
	if (Slot.IsEmpty() || Slot.Len() > 128)
		return false;
	for (TCHAR Character : Slot)
		if (!FChar::IsAlnum(Character) && Character != TCHAR('_') && Character != TCHAR('-'))
			return false;
	return true;
}
} // namespace Nelaric::CommandSave

bool UDemoCommandLibrary::SaveCommands(const FString& Slot, const FString& Snapshot)
{
	check(IsInGameThread());
	if (!Nelaric::CommandSave::ValidSlot(Slot) || Snapshot.Len() > 1048576 || !Snapshot.StartsWith(TEXT("{")))
		return false;
	auto* Save = NewObject<UDemoCommandSaveGame>();
	Save->Snapshot = Snapshot;
	return UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
}

FString UDemoCommandLibrary::LoadCommands(const FString& Slot)
{
	check(IsInGameThread());
	if (!Nelaric::CommandSave::ValidSlot(Slot))
		return {};
	auto* Save = Cast<UDemoCommandSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
	return Save && Save->Snapshot.Len() <= 1048576 ? Save->Snapshot : FString();
}

UDemoCompanyDefinition::UDemoCompanyDefinition()
{
	CompanyId = TEXT("Company");
}
