// Copyright (c) 2026 Nelaric Contributors

/** @file DemoGrandWarfront.h Declares battlefront world and body adapters. */
#pragma once
#include "DemoControlGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "ObjectPool/DemoCharacterPoolSubsystem.h"
#include "DemoGrandWarfront.generated.h"

/// One world-owned TS lifecycle receiver.
DECLARE_DYNAMIC_DELEGATE(FDemoBattlefrontPulse);

class ADemoCommandArea;
class ADemoRuntimeCharacterSpawnPoint;
class ADemoSquadCommandActor;

/// Replicated complete match view; late joiners require no event replay.
UCLASS(MinimalAPI)
class ADemoGrandWarfrontState : public AGameStateBase
{
	GENERATED_BODY()
public:
	/// Committed public JSON, updated only by the authority game mode.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Demo|Battlefront")
	FString Snapshot;

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

/// Default match display, reading only the replicated public snapshot.
UCLASS(MinimalAPI)
class ADemoGrandWarfrontHUD : public AHUD
{
	GENERATED_BODY()
public:
public:
	virtual void DrawHUD() override;
};

/// Authority lifecycle and native adapters; TS owns scoring and scheduling.
UCLASS(MinimalAPI)
class ADemoGrandWarfrontGameMode : public ADemoControlGameMode
{
	GENERATED_BODY()
public:
	/// Reads the packaged demo definition once at startup; game thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	FString ReadDefinition() const;
	/// Creates one unique sensor and optional spawn cache; authority thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	bool CreateArea(const FString& Id, FVector Center, float Radius, float HalfHeight, int32 SpawnTeam);
	/// Connects authored same-region navigation passages; authority thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	bool ConnectAreas(const FString& From, const FString& To);
	/// Samples valid pooled bodies once per sensor; empty on invalid input.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	FString SamplePoints(const TArray<FString>& Ids, uint8 Attacker, uint8 Defender);
	/// Acquires a bounded seat lease and joins its squad; authority thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	bool SpawnSeat(const FString& Seat, const FString& SpawnArea, uint8 Team, ADemoSquadCommandActor* Squad);
	/// Spawns one complete batch or rolls back acquired leases; authority thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	bool SpawnReinforcements(const TArray<FString>& SeatIds, const FString& SpawnArea, uint8 Team);
	/// Returns the exact current seat body, or null; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Battlefront")
	ADemoCharacter* GetSeatBody(const FString& Seat) const;
	/// Releases only a dead seat, returning player control first; game thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	bool ReleaseDeadSeat(const FString& Seat);
	/// Moves a live frozen seat to a valid cached location; game thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	bool DeploySeat(const FString& Seat, const FString& SpawnArea);
	/// Freezes movement, weapons and incoming damage for owned live seats.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	void FreezeSeats(bool bFrozen);
	/// Commits a bounded snapshot for replication; authority thread only.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	void PublishSnapshot(const FString& Snapshot);
	/// Selects the two participating teams before assigning players.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	void ConfigureTeams(uint8 Attacker, uint8 Defender);
	/// Selects current spawn areas and moves each owner camera; authority thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Battlefront")
	void SetActiveSpawnAreas(const FString& AttackerSpawn, const FString& DefenderSpawn);

public:
	ADemoGrandWarfrontGameMode();
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	UPROPERTY(Transient)
	FDemoBattlefrontPulse UpdateHandler;
	UPROPERTY(Transient)
	FDemoBattlefrontPulse StopHandler;
	virtual void StartPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer,
	                                                          const FTransform& SpawnTransform) override;
	virtual void PostLogin(APlayerController* Player) override;
	virtual bool CanChangePawnControl_Implementation(AController* Requester, EControlSwitchAction Action,
	                                                 APawn* Target) const override;
	UFUNCTION(BlueprintNativeEvent)
	void TickBattlefront();
	virtual void TickBattlefront_Implementation();
	UFUNCTION(BlueprintNativeEvent)
	void StopBattlefront();
	virtual void StopBattlefront_Implementation();

private:
	struct FSeat
	{
		UDemoCharacterPoolSubsystem::FHandle Handle;
		TWeakObjectPtr<ADemoSquadCommandActor> Squad;
		uint8 Team = 255;
	};
	TMap<FString, FSeat> Seats;
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<ADemoRuntimeCharacterSpawnPoint>> SpawnAreas;
	UPROPERTY(Transient)
	TMap<FString, TObjectPtr<ADemoCommandArea>> Areas;
	FTimerHandle Pulse;
	bool bFrozen = true;
	uint8 AttackerTeam = 0;
	uint8 DefenderTeam = 1;
	int32 JoinedPlayers = 0;
	FString ActiveAttackerSpawn;
	FString ActiveDefenderSpawn;
};
