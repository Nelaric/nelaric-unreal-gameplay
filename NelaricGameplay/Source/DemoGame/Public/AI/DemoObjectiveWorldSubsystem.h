// Copyright (c) 2026 Nelaric Contributors

/** @file DemoObjectiveWorldSubsystem.h Declares spatial rule input adapters. */
#pragma once

#include "AI/DemoCommandTypes.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "DemoObjectiveWorldSubsystem.generated.h"

class UBoxComponent;
class ADemoCharacter;
class UPrimitiveComponent;
class ADemoSquadCommandActor;

/// Authored passage capacity and availability, shared by command tiers.
USTRUCT(BlueprintType)
struct FDemoCommandPassage
{
	GENERATED_BODY()
	/// Shared resource identity; opposite directions may share this identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	FString Id;
	/// Destination area identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	FString To;
	/// Maximum simultaneous platoon claims.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	int32 Capacity = 1;
	/// Rule-authorized availability; independent of hidden enemy state.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	bool bAvailable = true;
	/// Opening world time; zero permits immediate use.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	double OpensAt = 0.0;
	/// Closing world time; zero leaves the window open.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	double ClosesAt = 0.0;
};

/// Rule sensor for one existing circular area; no tactical decisions.
UCLASS(MinimalAPI, Blueprintable)
class ADemoCommandArea : public AActor
{
	GENERATED_BODY()
public:
	/// Unique shared tactical area identity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	FString AreaId;
	/// Existing circular centimeter geometry, centered on this actor.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	FDemoSquadArea Area;
	/// Vertical sensor extent in centimeters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	float HalfHeight = 500.0f;
	/// Maximum simultaneous platoon responsibilities.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	int32 Capacity = 8;
	/// Authored passages to other shared tactical areas.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
	TArray<FDemoCommandPassage> Passages;
	/// Records bounded search coverage from an exact squad task; game thread.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Demo|Objective")
	bool RecordSearch(ADemoSquadCommandActor* Squad, FGuid MissionId, int32 Revision);

public:
	ADemoCommandArea();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	void RefreshOccupants();
	const TArray<TWeakObjectPtr<ADemoCharacter>>& GetOccupants() const;
	const TSet<FString>& GetSearchScopes(uint8 TeamId) const;
	int32 GetSearchCount(uint8 TeamId, const FString& Scope) const;

private:
	UPROPERTY()
	TObjectPtr<UBoxComponent> Sensor;
	TArray<TWeakObjectPtr<ADemoCharacter>> Occupants;
	TMap<uint8, TSet<FString>> SearchScopes;
	TMap<uint8, TMap<FString, TSet<FString>>> SearchPlatoons;
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComponent,
	                   int32 BodyIndex, bool bSweep, const FHitResult& Hit);
	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* Overlapped, AActor* Other, UPrimitiveComponent* OtherComponent,
	                      int32 BodyIndex);
};

/// Authority-side rule facts. Company knowledge receives only rule results.
UCLASS(MinimalAPI)
class UDemoObjectiveWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	/// Returns shared geometry and capacity definitions as bounded JSON.
	UFUNCTION(BlueprintPure, Category = "Demo|Objective")
	FString GetAreaDefinitions() const;
	/// Returns a borrowed shared area sensor, or null; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Objective")
	ADemoCommandArea* FindArea(const FString& AreaId) const;
	/** @brief Samples only registered spatial sensors; authority game thread.
	 * @param TeamId Friendly rule team; player-controlled soldiers count.
	 * @param Participants Explicit friendly identities needed for escort rules.
	 * @return Private evaluator inputs without hostile positions or attributes.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Objective")
	FString GetRuleInputs(uint8 TeamId, const TArray<FString>& Participants);
	/// Returns a stable rule participant manifest from an authorized platoon.
	UFUNCTION(BlueprintPure, Category = "Demo|Objective")
	TArray<FString> GetPlatoonParticipants(AActor* Platoon, uint8 TeamId) const;
	/// Registers a stable soldier binding for rule evaluation; game thread.
	void RegisterSoldier(FGuid UnitId, ADemoCharacter* Soldier);
	/// Invalidates only the matching body binding, retaining committed death.
	void UnregisterSoldier(FGuid UnitId, ADemoCharacter* Soldier);

public:
	bool RegisterArea(ADemoCommandArea* Area);
	void NotifyFactsChanged();
	bool CanBindSoldier(FGuid UnitId, ADemoCharacter* Soldier) const;
	void RecordSoldierDeath(FGuid UnitId);
	void UnregisterArea(ADemoCommandArea* Area);
	virtual void Deinitialize() override;

protected:
	/// Runtime worlds only; rule authority never executes in editor previews.
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	TMap<FString, TWeakObjectPtr<ADemoCommandArea>> Areas;
	TMap<FString, TWeakObjectPtr<ADemoCharacter>> Soldiers;
	TSet<FString> CommittedDeaths;
};
