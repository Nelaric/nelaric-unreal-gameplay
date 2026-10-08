// Copyright (c) 2026 Nelaric Contributors

#include "Gameplay/DemoGrandWarfront.h"
#include "AI/DemoObjectiveWorldSubsystem.h"
#include "AI/DemoSquadCommandActor.h"
#include "AI/DemoSquadMemberComponent.h"
#include "Spawning/DemoRuntimeCharacterSpawnPoint.h"
#include "Player/DemoPlayerController.h"
#include "Player/DemoOverviewPawn.h"
#include "GAS/DemoGasPlayerState.h"
#include "Player/ControlSwitchSubsystem.h"
#include "HAL/FileManager.h"
#include "Components/BoxComponent.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NavigationSystem.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Kismet/GameplayStatics.h"
#include "Scripting/DemoScriptSubsystem.h"
#include "Engine/Engine.h"

void ADemoGrandWarfrontState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADemoGrandWarfrontState, Snapshot);
}
void ADemoGrandWarfrontHUD::DrawHUD()
{
	Super::DrawHUD();
	const auto* State = GetWorld()->GetGameState<ADemoGrandWarfrontState>();
	if (!Canvas || !State)
		return;
	TSharedPtr<FJsonObject> Data;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(State->Snapshot), Data) || !Data)
		return;
	FString Phase, Region, Reason;
	double Deadline = 0, Winner = 255;
	Data->TryGetStringField(TEXT("phase"), Phase);
	Data->TryGetStringField(TEXT("regionId"), Region);
	Data->TryGetStringField(TEXT("reason"), Reason);
	Data->TryGetNumberField(TEXT("deadline"), Deadline);
	if (Phase == TEXT("Preparing") || Phase == TEXT("RegionTransition"))
		Data->TryGetNumberField(TEXT("stageEndsAt"), Deadline);
	Data->TryGetNumberField(TEXT("winnerTeamId"), Winner);
	float Y = 36;
	DrawText(FString::Printf(TEXT("Grand Warfront | %s | %s | %.0fs"), *Region, *Phase,
	                         FMath::Max(0.0, Deadline - State->GetServerWorldTimeSeconds())),
	         FColor::White, 36, Y);
	Y += 24;
	const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
	if (Data->TryGetArrayField(TEXT("points"), Points))
		for (const auto& Value : *Points)
		{
			const auto Point = Value->AsObject();
			if (!Point)
				continue;
			FString Id;
			double Score = 0, PointOwner = 255, Attackers = 0, Defenders = 0, AttackerTeam = 0;
			Point->TryGetStringField(TEXT("id"), Id);
			Point->TryGetNumberField(TEXT("attackScore"), Score);
			Point->TryGetNumberField(TEXT("ownerTeamId"), PointOwner);
			Point->TryGetNumberField(TEXT("attackers"), Attackers);
			Point->TryGetNumberField(TEXT("defenders"), Defenders);
			Data->TryGetNumberField(TEXT("attackerTeamId"), AttackerTeam);
			const FColor Color = PointOwner == AttackerTeam ? FColor(70, 165, 255) : FColor(255, 125, 70);
			const TCHAR* Status = Attackers == Defenders  ? (Attackers > 0 ? TEXT("CONTESTED") : TEXT("EMPTY"))
			                      : Attackers > Defenders ? TEXT("ATTACKERS +1/s")
			                                              : TEXT("DEFENDERS +1/s");
			double X = 0, WorldY = 0, Z = 0, Radius = 0;
			if (Point->TryGetNumberField(TEXT("worldX"), X) && Point->TryGetNumberField(TEXT("worldY"), WorldY) &&
			    Point->TryGetNumberField(TEXT("worldZ"), Z) && Point->TryGetNumberField(TEXT("radius"), Radius))
			{
				const FVector Center(X, WorldY, Z - 80);
				// Project the public capture footprint; this is not a debug-only draw call.
				for (int32 Segment = 0; Segment < 48; ++Segment)
				{
					const float A = Segment * 2 * PI / 48, B = (Segment + 1) * 2 * PI / 48;
					const FVector P = Project(Center + FVector(FMath::Cos(A), FMath::Sin(A), 0) * Radius);
					const FVector Q = Project(Center + FVector(FMath::Cos(B), FMath::Sin(B), 0) * Radius);
					if (P.Z > 0 && Q.Z > 0)
						DrawLine(P.X, P.Y, Q.X, Q.Y, Color, 3);
				}
				const FVector Marker = Project(Center + FVector(0, 0, 220));
				if (Marker.Z > 0)
				{
					const float LabelX = FMath::Clamp(float(Marker.X), 30.0f, FMath::Max(30.0f, Canvas->ClipX - 260));
					const float LabelY = FMath::Clamp(float(Marker.Y), 190.0f, FMath::Max(190.0f, Canvas->ClipY - 70));
					DrawRect(FLinearColor(0, 0, 0, 0.65f), LabelX - 4, LabelY - 3, 254, 47);
					DrawText(FString::Printf(TEXT("%s  ATT %.0f : DEF %.0f"), *Id, Attackers, Defenders), Color, LabelX,
					         LabelY);
					DrawText(FString::Printf(TEXT("%.0f / 60   %s"), Score, Status), FColor::White, LabelX,
					         LabelY + 20);
				}
			}
			DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), 36, Y, 240, 18);
			DrawRect(FLinearColor(0.2f, 0.5f, 1, 0.8f), 36, Y, 240 * FMath::Clamp(Score / 60, 0.0, 1.0), 18);
			DrawText(FString::Printf(TEXT("%s  ATT %.0f / DEF %.0f  owner %.0f"), *Id, Score, 60 - Score, PointOwner),
			         FColor::White, 40, Y);
			Y += 24;
			DrawText(FString::Printf(TEXT("In point: ATT %.0f / DEF %.0f | %s"), Attackers, Defenders, Status), Color,
			         40, Y);
			Y += 24;
		}
	if (Winner != 255)
		DrawText(FString::Printf(TEXT("Team %.0f wins"), Winner), FColor::Yellow, 36, Y);
	else if (!Reason.IsEmpty())
		DrawText(Reason, FColor::Yellow, 36, Y);
}
ADemoGrandWarfrontGameMode::ADemoGrandWarfrontGameMode()
{
	GameStateClass = ADemoGrandWarfrontState::StaticClass();
	HUDClass = ADemoGrandWarfrontHUD::StaticClass();
}
FString ADemoGrandWarfrontGameMode::ReadDefinition() const
{
	FString Text;
	const FString Path = FPaths::ProjectContentDir() / TEXT("Demo/Demo1_GrandWarfront/Config/GrandWarfront.json");
	if (IFileManager::Get().FileSize(*Path) > 262144 || !FFileHelper::LoadFileToString(Text, *Path))
		return {};
	return Text;
}
void ADemoGrandWarfrontGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	if (auto* Class = LoadClass<ADemoPlayerController>(
	        nullptr, TEXT("/Game/Demo/Demo1_GrandWarfront/Player/BP_DemoPlayerController.BP_DemoPlayerController_C")))
		PlayerControllerClass = Class;
	if (auto* Class = LoadClass<ADemoOverviewPawn>(
	        nullptr, TEXT("/Game/Demo/Demo1_GrandWarfront/Player/BP_DemoOverviewPawn.BP_DemoOverviewPawn_C")))
		DefaultPawnClass = Class;
}
void ADemoGrandWarfrontGameMode::StartPlay()
{
	Super::StartPlay();
	if (auto* Scripts = GEngine ? GEngine->GetEngineSubsystem<UDemoScriptSubsystem>() : nullptr)
		Scripts->StartBattlefrontHandler.ExecuteIfBound(this);
	GetWorldTimerManager().SetTimer(Pulse, this, &ThisClass::TickBattlefront, 0.1f, true);
}
void ADemoGrandWarfrontGameMode::TickBattlefront_Implementation()
{
	if (UpdateHandler.IsBound())
	{
		UpdateHandler.Execute();
		return;
	}
	UE_LOG(LogTemp, Error, TEXT("[Warfront] TypeScript entry unavailable."));
	PublishSnapshot(TEXT("{\"phase\":\"Aborted\",\"reason\":\"TypeScript entry unavailable\"}"));
	GetWorldTimerManager().ClearTimer(Pulse);
}
void ADemoGrandWarfrontGameMode::StopBattlefront_Implementation()
{
	if (StopHandler.IsBound())
		StopHandler.Execute();
}
void ADemoGrandWarfrontGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(Pulse);
	StopBattlefront();
	UpdateHandler.Unbind();
	StopHandler.Unbind();
	Seats.Reset();
	SpawnAreas.Reset();
	Areas.Reset();
	Super::EndPlay(Reason);
}
void ADemoGrandWarfrontGameMode::SetActiveSpawnAreas(const FString& AttackerSpawn, const FString& DefenderSpawn)
{
	if (!HasAuthority() || !SpawnAreas.Contains(AttackerSpawn) || !SpawnAreas.Contains(DefenderSpawn))
		return;
	ActiveAttackerSpawn = AttackerSpawn;
	ActiveDefenderSpawn = DefenderSpawn;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (auto* Player = Cast<ADemoPlayerController>(It->Get()))
			if (auto* Camera = Player->GetOverviewPawn())
				if (const auto* State = Player->GetPlayerState<ADemoGasPlayerState>())
					if (const auto* Area = Areas
					                           .FindRef(State->BattlefrontTeamId == AttackerTeam ? ActiveAttackerSpawn
					                                                                             : ActiveDefenderSpawn)
					                           .Get())
					{
						const FVector Location = Area->Area.Center + OverviewSpawnOffset;
						Camera->SetActorLocation(Location);
						Camera->ClientDeployOverview(Location);
					}
}
APawn* ADemoGrandWarfrontGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer,
                                                                              const FTransform& SpawnTransform)
{
	FTransform Transform = SpawnTransform;
	if (const auto* State = NewPlayer ? NewPlayer->GetPlayerState<ADemoGasPlayerState>() : nullptr)
		if (const auto* Area =
		        Areas.FindRef(State->BattlefrontTeamId == AttackerTeam ? ActiveAttackerSpawn : ActiveDefenderSpawn)
		            .Get())
			Transform.SetLocation(Area->Area.Center);
	return Super::SpawnDefaultPawnAtTransform_Implementation(NewPlayer, Transform);
}
bool ADemoGrandWarfrontGameMode::CreateArea(const FString& Id, FVector Center, float Radius, float HalfHeight,
                                            int32 SpawnTeam)
{
	if (!HasAuthority() || Id.IsEmpty() || Id.Len() > 128 || Areas.Num() >= 198 || Areas.Contains(Id) ||
	    Center.ContainsNaN() || !FMath::IsFinite(Radius) || Radius < 100 || !FMath::IsFinite(HalfHeight) ||
	    HalfHeight < 100 || SpawnTeam < -1 || SpawnTeam > 254)
		return false;
	auto* Rules = GetWorld()->GetSubsystem<UDemoObjectiveWorldSubsystem>();
	if (!Rules || Rules->FindArea(Id))
		return false;
	auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (!Navigation || !Navigation->ProjectPointToNavigation(Center, Projected, FVector(300, 300, 3000)))
		return false;
	Center = Projected.Location + FVector(0, 0, 100);
	for (const auto& Entry : Areas)
		if (IsValid(Entry.Value) &&
		    FVector::DistSquared2D(Center, Entry.Value->Area.Center) <
		        FMath::Square(Radius + Entry.Value->Area.Radius) &&
		    FMath::Abs(Center.Z - Entry.Value->Area.Center.Z) < HalfHeight + Entry.Value->HalfHeight)
			return false;
	const FTransform Transform(Center);
	auto* Area = GetWorld()->SpawnActorDeferred<ADemoCommandArea>(ADemoCommandArea::StaticClass(), Transform, this);
	if (!Area)
		return false;
	Area->AreaId = Id;
	Area->Area.Radius = Radius;
	Area->HalfHeight = HalfHeight;
	Area->Capacity = 64;
	Area->FinishSpawning(Transform);
	Areas.Add(Id, Area);
	if (SpawnTeam >= 0)
	{
		auto* Spawn = GetWorld()->SpawnActorDeferred<ADemoRuntimeCharacterSpawnPoint>(
		    ADemoRuntimeCharacterSpawnPoint::StaticClass(), Transform, this);
		if (!Spawn)
			return false;
		Spawn->TeamId = uint8(SpawnTeam);
		Spawn->SpawnArea->SetBoxExtent(FVector(Radius * 0.6, Radius * 0.6, HalfHeight));
		Spawn->FinishSpawning(Transform);
		SpawnAreas.Add(Id, Spawn);
	}
	return true;
}
bool ADemoGrandWarfrontGameMode::ConnectAreas(const FString& From, const FString& To)
{
	auto* A = Areas.FindRef(From).Get();
	auto* B = Areas.FindRef(To).Get();
	if (!HasAuthority() || !IsValid(A) || !IsValid(B) || A == B || A->Passages.Num() >= 66)
		return false;
	FDemoCommandPassage Passage;
	Passage.Id = From < To ? From + TEXT(":") + To : To + TEXT(":") + From;
	Passage.To = To;
	Passage.Capacity = 64;
	A->Passages.Add(Passage);
	return true;
}
FString ADemoGrandWarfrontGameMode::SamplePoints(const TArray<FString>& Ids, uint8 Attacker, uint8 Defender)
{
	if (!HasAuthority() || Ids.IsEmpty() || Ids.Num() > 64 || Attacker == Defender || Attacker == 255 ||
	    Defender == 255)
		return {};
	auto Root = MakeShared<FJsonObject>();
	auto* Pool = GetWorld()->GetSubsystem<UDemoCharacterPoolSubsystem>();
	if (!Pool || !Pool->IsReady())
		return {};
	for (const auto& Id : Ids)
	{
		auto* Area = Areas.FindRef(Id).Get();
		if (!IsValid(Area))
			return {};
		Area->RefreshOccupants();
		int32 Attack = 0, Defend = 0;
		for (const auto& Occupant : Area->GetOccupants())
		{
			auto* Body = Occupant.Get();
			if (!IsValid(Body) || !Body->IsPoolActive() || !Body->IsAlive() || Body->HasCommittedDeath())
				continue;
			bool bLeased = false;
			for (const auto& Seat : Seats)
				if (Pool->Get(Seat.Value.Handle) == Body)
				{
					bLeased = true;
					break;
				}
			if (!bLeased ||
			    FVector::DistSquared2D(Body->GetActorLocation(), Area->Area.Center) >
			        FMath::Square(Area->Area.Radius) ||
			    FMath::Abs(Body->GetActorLocation().Z - Area->Area.Center.Z) > Area->HalfHeight)
				continue;
			if (Body->GetTeamId() == Attacker)
				++Attack;
			else if (Body->GetTeamId() == Defender)
				++Defend;
		}
		auto Counts = MakeShared<FJsonObject>();
		Counts->SetNumberField(TEXT("attack"), Attack);
		Counts->SetNumberField(TEXT("defend"), Defend);
		Root->SetObjectField(Id, Counts);
	}
	FString Json;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
	return Json;
}
ADemoCharacter* ADemoGrandWarfrontGameMode::GetSeatBody(const FString& Seat) const
{
	auto* Pool = GetWorld()->GetSubsystem<UDemoCharacterPoolSubsystem>();
	const auto* Existing = Seats.Find(Seat);
	return Pool && Existing ? Pool->Get(Existing->Handle) : nullptr;
}
bool ADemoGrandWarfrontGameMode::SpawnSeat(const FString& Seat, const FString& SpawnArea, uint8 Team,
                                           ADemoSquadCommandActor* Squad)
{
	auto* Spawn = SpawnAreas.FindRef(SpawnArea).Get();
	auto* Pool = GetWorld()->GetSubsystem<UDemoCharacterPoolSubsystem>();
	if (!HasAuthority() || Seat.IsEmpty() || Seat.Len() > 128 || !IsValid(Squad) || Squad->GetWorld() != GetWorld() ||
	    !IsValid(Spawn) || Spawn->TeamId != Team || !Pool || !Pool->IsReady() || Spawn->GetSpawnLocations().IsEmpty())
		return false;
	auto* Existing = Seats.Find(Seat);
	if (Existing && (Existing->Team != Team || Existing->Squad.Get() != Squad))
		return false;
	if (auto* Body = GetSeatBody(Seat))
		return Body->IsAlive();
	if (!Existing && Seats.Num() >= UDemoCharacterPoolSubsystem::Capacity)
		return false;
	FTransform Transform;
	if (Spawn->FindSpawnTransform(Transform) != EDemoRuntimeCharacterSpawnResult::Spawned)
		return false;
	const auto Lease = Pool->TryAcquire(Transform, Team);
	if (!Lease)
		return false;
	auto* Member = Lease.Object->FindComponentByClass<UDemoSquadMemberComponent>();
	if (!Member || !Member->JoinSquad(Squad, FGuid::NewGuid()))
	{
		(void)Pool->Release(Lease.Handle);
		return false;
	}
	FSeat Binding;
	Binding.Handle = Lease.Handle;
	Binding.Team = Team;
	Binding.Squad = Squad;
	Seats.Add(Seat, Binding);
	Lease.Object->SetBattlefrontFrozen(bFrozen);
	return true;
}
bool ADemoGrandWarfrontGameMode::SpawnReinforcements(const TArray<FString>& SeatIds, const FString& SpawnArea,
                                                     uint8 Team)
{
	if (!HasAuthority() || SeatIds.IsEmpty() || SeatIds.Num() > 16)
		return false;
	auto* Pool = GetWorld()->GetSubsystem<UDemoCharacterPoolSubsystem>();
	if (!Pool)
		return false;
	TSet<FString> Unique;
	for (const auto& Id : SeatIds)
	{
		const auto* Seat = Seats.Find(Id);
		if (!Seat || Seat->Team != Team || !Seat->Squad.IsValid() || GetSeatBody(Id) || Unique.Contains(Id))
			return false;
		Unique.Add(Id);
	}
	TArray<FString> Acquired;
	for (const auto& Id : SeatIds)
	{
		if (!SpawnSeat(Id, SpawnArea, Team, Seats.FindChecked(Id).Squad.Get()))
		{
			for (const auto& Rollback : Acquired)
			{
				auto& Seat = Seats.FindChecked(Rollback);
				(void)Pool->Release(Seat.Handle);
				Seat.Handle = {};
			}
			return false;
		}
		Acquired.Add(Id);
		GetSeatBody(Id)->SetBattlefrontFrozen(true);
	}
	// All bodies enter play in this game-thread turn, after the batch succeeds.
	for (const auto& Id : Acquired)
		GetSeatBody(Id)->SetBattlefrontFrozen(bFrozen);
	return true;
}
bool ADemoGrandWarfrontGameMode::ReleaseDeadSeat(const FString& Seat)
{
	if (!HasAuthority())
		return false;
	auto* Body = GetSeatBody(Seat);
	if (!Body)
		return true;
	if (Body->IsAlive())
		return false;
	if (auto* Player = Cast<ADemoPlayerController>(Body->GetController()))
	{
		auto* Control = GetWorld()->GetSubsystem<UControlSwitchSubsystem>();
		if (!Control || !Player->GetOverviewPawn() ||
		    Control->ExecuteControlSwitch(Player, EControlSwitchAction::TakeControl, Player->GetOverviewPawn()) !=
		        EControlSwitchResult::Succeeded)
			return false;
	}
	auto* Pool = GetWorld()->GetSubsystem<UDemoCharacterPoolSubsystem>();
	if (!Pool || !Pool->ReleaseDeadCharacter(Body))
		return false;
	Seats.FindChecked(Seat).Handle = {};
	return true;
}
bool ADemoGrandWarfrontGameMode::DeploySeat(const FString& Seat, const FString& SpawnArea)
{
	auto* Body = GetSeatBody(Seat);
	auto* Spawn = SpawnAreas.FindRef(SpawnArea).Get();
	if (!HasAuthority() || !bFrozen || !IsValid(Body) || !Body->IsAlive() || !IsValid(Spawn) ||
	    Spawn->TeamId != Body->GetTeamId() || Spawn->GetSpawnLocations().IsEmpty())
		return false;
	FTransform Transform;
	if (Spawn->FindSpawnTransform(Transform) != EDemoRuntimeCharacterSpawnResult::Spawned)
		return false;
	const bool bMoved = Body->TeleportTo(Transform.GetLocation(), Transform.Rotator(), false, false);
	Body->SetBattlefrontFrozen(true);
	return bMoved;
}
void ADemoGrandWarfrontGameMode::FreezeSeats(bool bInFrozen)
{
	if (!HasAuthority())
		return;
	bFrozen = bInFrozen;
	for (const auto& Seat : Seats)
		if (auto* Body = GetSeatBody(Seat.Key))
			Body->SetBattlefrontFrozen(bFrozen);
}
void ADemoGrandWarfrontGameMode::PublishSnapshot(const FString& Snapshot)
{
	if (!HasAuthority() || Snapshot.Len() > 65536)
		return;
	if (auto* State = GetGameState<ADemoGrandWarfrontState>())
	{
		TSharedPtr<FJsonObject> Data;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Snapshot), Data) || !Data)
			return;
		const TArray<TSharedPtr<FJsonValue>>* Points = nullptr;
		if (Data->TryGetArrayField(TEXT("points"), Points))
			for (const auto& Value : *Points)
			{
				const auto Point = Value->AsObject();
				FString Id;
				if (!Point || !Point->TryGetStringField(TEXT("areaId"), Id))
					continue;
				if (const auto* Area = Areas.FindRef(Id).Get())
				{
					Point->SetNumberField(TEXT("worldX"), Area->Area.Center.X);
					Point->SetNumberField(TEXT("worldY"), Area->Area.Center.Y);
					Point->SetNumberField(TEXT("worldZ"), Area->Area.Center.Z);
					Point->SetNumberField(TEXT("radius"), Area->Area.Radius);
				}
			}
		FString PublicSnapshot;
		FJsonSerializer::Serialize(Data.ToSharedRef(), TJsonWriterFactory<>::Create(&PublicSnapshot));
		State->Snapshot = PublicSnapshot;
		State->ForceNetUpdate();
	}
}
void ADemoGrandWarfrontGameMode::ConfigureTeams(uint8 Attacker, uint8 Defender)
{
	if (Attacker == Defender || Attacker == 255 || Defender == 255)
		return;
	AttackerTeam = Attacker;
	DefenderTeam = Defender;
	JoinedPlayers = 0;
	for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		if (auto* Player = It->Get())
		{
			const uint8 Team = (JoinedPlayers++ % 2) == 0 ? AttackerTeam : DefenderTeam;
			if (auto* State = Player->GetPlayerState<ADemoGasPlayerState>())
				State->BattlefrontTeamId = Team;
			if (auto* Controller = Cast<ADemoPlayerController>(Player))
				Controller->CompanyCommandTeamId = Team;
		}
}
void ADemoGrandWarfrontGameMode::PostLogin(APlayerController* Player)
{
	Super::PostLogin(Player);
	const auto* ExistingState = Player->GetPlayerState<ADemoGasPlayerState>();
	const uint8 Team = ExistingState && (ExistingState->BattlefrontTeamId == AttackerTeam ||
	                                     ExistingState->BattlefrontTeamId == DefenderTeam)
	                       ? ExistingState->BattlefrontTeamId
	                       : ((JoinedPlayers++ % 2) == 0 ? AttackerTeam : DefenderTeam);
	if (auto* State = Player->GetPlayerState<ADemoGasPlayerState>())
		State->BattlefrontTeamId = Team;
	if (auto* Controller = Cast<ADemoPlayerController>(Player))
		Controller->CompanyCommandTeamId = Team;
}
bool ADemoGrandWarfrontGameMode::CanChangePawnControl_Implementation(AController* Requester,
                                                                     EControlSwitchAction Action, APawn* Target) const
{
	if (!Super::CanChangePawnControl_Implementation(Requester, Action, Target))
		return false;
	if (const auto* Body = Cast<ADemoCharacter>(Target); Body && Requester && Requester->IsPlayerController())
	{
		const auto* State = Requester ? Requester->GetPlayerState<ADemoGasPlayerState>() : nullptr;
		return State && State->BattlefrontTeamId == Body->GetTeamId() && !bFrozen && Body->IsAlive();
	}
	return true;
}
