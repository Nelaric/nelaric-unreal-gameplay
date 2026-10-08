// Copyright (c) 2026 Nelaric Contributors

/** @file DemoCompanyRegistrySubsystem.h Declares command ownership. */
#pragma once

#include "AI/DemoCommandTypes.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Subsystems/WorldSubsystem.h"
#include "DemoCompanyRegistrySubsystem.generated.h"

class UDemoObjectiveWorldSubsystem;
class ADemoCompanyCommandActor;
class ADemoSquadCommandActor;

/// Authority-world team registry; contains no allocation decisions.
UCLASS(MinimalAPI)
class UDemoCompanyRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()
public:
	/** @brief Claims one team command publication lease; game thread only.
	 * @param Actor Live same-world authority actor.
	 * @param CompanyId Stable nonempty logical identity.
	 * @param TeamId Non-neutral faction; defaults to the legacy team zero.
	 * @return Positive lease generation, or zero for a duplicate or client.
	 */
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	int32 RegisterCompany(AActor* Actor, const FString& CompanyId, uint8 TeamId = 0);
	/// Releases only the exact publication lease; authority game thread.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	bool UnregisterCompany(AActor* Actor, int32 Epoch);
	/// Returns the borrowed valid command actor, or null; game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	AActor* GetCompany(uint8 TeamId = 0) const;
	/// Returns all live team publishers; borrowed actors, game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	TArray<AActor*> GetCompanies() const;
	/// Checks current team registration; borrowed actor, game thread only.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	bool IsRegisteredCompany(AActor* Actor) const;
	/// Returns whether this exact actor owns the publication lease.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	bool HasPublicationAuthority(AActor* Actor, int32 Epoch) const;
	/// Returns the unique current-world execution identity on the game thread.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	FString GetRunId() const;

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

protected:
	/// Supports independent runtime and PIE worlds, excluding editor previews.
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

private:
	TMap<uint8, TWeakObjectPtr<AActor>> Companies;
	TMap<uint8, FString> Identities;
	TMap<uint8, int32> Epochs;
	FString RunId;
	int32 Generation = 0;
	bool bStopping = false;
};

/// Typed reflection and save adapters; coordination remains in TypeScript.
UCLASS(MinimalAPI)
class UDemoCommandLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/// Borrows the executing world's registry; game thread only.
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "Demo|Company")
	static UDemoCompanyRegistrySubsystem* GetRegistry(UObject* WorldContext);
	/** @brief Creates a team publisher from a uniform battlefield entry.
	 * @param WorldContext Authority runtime
	 * world context; game thread only.
	 * @param CommandClass Authored virtual company class and StateTree.
	 *
	 * @param Definition Stable company identity and complete platoon manifest.
	 * @param Policy Scheduling,
	 * allocation and repair limits.
	 * @param Mission Optional externally authored initial company mission.
	 *
	 * @param Platoons Explicit same-world virtual platoon objects.
	 * @return Borrowed matching company, or null
	 * for invalid startup.
	 */
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContext"), Category = "Demo|Company")
	static ADemoCompanyCommandActor* CreateCompany(UObject* WorldContext,
	                                               TSubclassOf<ADemoCompanyCommandActor> CommandClass,
	                                               UDemoCompanyDefinition* Definition, UDemoCompanyPolicy* Policy,
	                                               UDemoCompanyMissionAsset* Mission, const TArray<AActor*>& Platoons);
	/// Borrows authority rule sensors for the executing world; game thread.
	UFUNCTION(BlueprintPure, meta = (WorldContext = "WorldContext"), Category = "Demo|Company")
	static UDemoObjectiveWorldSubsystem* GetObjectives(UObject* WorldContext);
	/// Serializes typed company intent for the TypeScript coordinator.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	static FString EncodeCompanyMission(const FDemoCompanyMission& Mission);
	/// Serializes typed platoon intent for the shared TypeScript protocol.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	static FString EncodePlatoonMission(const FDemoPlatoonMission& Mission);
	/// Captures stable identities and symbolic body bindings, without handles.
	UFUNCTION(BlueprintPure, Category = "Demo|Company")
	static FString CaptureSquadBindings(ADemoSquadCommandActor* Squad);
	/// Validates owned bindings; optionally applies the complete accepted set.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	static bool RestoreSquadBindings(ADemoSquadCommandActor* Squad, const FString& Bindings, bool bApply = true);
	/// Writes a bounded semantic snapshot through Unreal's save-game API.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	static bool SaveCommands(const FString& Slot, const FString& Snapshot);
	/// Returns a saved semantic snapshot, or empty for a missing/invalid slot.
	UFUNCTION(BlueprintCallable, Category = "Demo|Company")
	static FString LoadCommands(const FString& Slot);

public:
};

/// Save envelope; stores no actors, delegates or navigation handles.
UCLASS(MinimalAPI)
class UDemoCommandSaveGame : public USaveGame
{
	GENERATED_BODY()
public:
	/// Versioned and validated command semantics in UTF-8 JSON form.
	UPROPERTY(SaveGame)
	FString Snapshot;

public:
};
