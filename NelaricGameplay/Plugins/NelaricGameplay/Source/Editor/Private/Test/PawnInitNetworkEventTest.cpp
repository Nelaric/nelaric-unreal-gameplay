// Copyright (c) 2026 Nelaric

#include "Test/PawnInitNetworkEventTestTypes.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Pawn/PawnInitializationComponent.h"
#include "Pawn/PawnInitializationConfig.h"

AInitNetworkEventPawn::AInitNetworkEventPawn(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bReplicates = true;
	bAlwaysRelevant = true;
	UPawnInitializationConfig* Config = CreateDefaultSubobject<UPawnInitializationConfig>(TEXT("NetworkInitConfig"));
	FPawnInitializationEntry Entry;
	Entry.ComponentId = TEXT("PlayerContext");
	Entry.ComponentClass = UInitNetworkEventComponent::StaticClass();
	Config->Components.Add(Entry);
	// Every peer gets this static fixture through its class defaults.
	GetPawnInitializationComponent()->InitializationConfig = Config;
}

bool UInitNetworkEventComponent::CanEntryReady()
{
	APawn* Pawn = GetPawn();
	APlayerState* State = GetPlayerState();
	if (!Pawn || !State)
	{
		return false;
	}
	if (Pawn->GetLocalRole() == ROLE_SimulatedProxy)
	{
		return true;
	}
	APlayerController* Controller = GetPlayerController();
	if (!Controller || Controller->PlayerState != State)
	{
		return false;
	}
	return !Pawn->IsLocallyControlled() || (Controller->GetLocalPlayer() && Pawn->InputComponent);
}

void UInitNetworkEventComponent::OnInitReady()
{
	++ReadyCalls;
	ReadyController = GetController();
	ReadyPlayerState = GetPlayerState();
	bHadLocalInput = GetPawn()->InputComponent != nullptr;
}

void UInitNetworkEventComponent::OnInitGenerationInvalidated(const Nelaric::FInitStateSnapshot& Previous)
{
	++CleanupCalls;
	ReadyController.Reset();
	ReadyPlayerState.Reset();
	bHadLocalInput = false;
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"

namespace Nelaric::Tests
{
TArray<UWorld*> NetworkWorlds()
{
	TArray<UWorld*> Result;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE && Context.World())
		{
			Result.Add(Context.World());
		}
	}
	return Result;
}

class FNetworkContextCommand final : public IAutomationLatentCommand
{
public:
	explicit FNetworkContextCommand(FAutomationTestBase& InTest) : Test(InTest)
	{
	}

	virtual bool Update() override
	{
		const TArray<UWorld*> Worlds = NetworkWorlds();
		UWorld* Server = nullptr;
		int32 Clients = 0;
		for (UWorld* World : Worlds)
		{
			if (World->GetNetMode() == NM_ListenServer)
			{
				Server = World;
			}
			Clients += World->GetNetMode() == NM_Client ? 1 : 0;
		}
		if (FPlatformTime::Seconds() - StartedAt > 60.0)
		{
			Test.AddError(
			    FString::Printf(TEXT("Network context phase %d timed out without polling initialization."), Phase));
			return true;
		}
		if (!Server || Clients != 2)
		{
			return false;
		}
		if (Phase == 0)
		{
			TArray<APlayerController*> RemoteControllers;
			APlayerController* LocalController = nullptr;
			for (TActorIterator<APlayerController> It(Server); It; ++It)
			{
				if (!It->PlayerState)
				{
					continue;
				}
				if (It->GetNetConnection())
				{
					RemoteControllers.Add(*It);
				}
				else if (It->GetLocalPlayer())
				{
					LocalController = *It;
				}
			}
			if (RemoteControllers.Num() != 2 || !LocalController)
			{
				return false;
			}
#if DO_ENABLE_NET_TEST
			for (UWorld* World : Worlds)
			{
				if (UNetDriver* Driver = World->GetNetDriver())
				{
					SavedPacketSettings.Add(Driver, Driver->PacketSimulationSettings);
					FPacketSimulationSettings Lag;
					Lag.PktLagMin = 50;
					Lag.PktLagMax = 150;
					Driver->SetPacketSimulationSettings(Lag);
				}
			}
#endif
			FirstController = RemoteControllers[0];
			SecondController = RemoteControllers[1];
			ServerPawn = Server->SpawnActor<AInitNetworkEventPawn>();
			LocalPawn = Server->SpawnActor<AInitNetworkEventPawn>();
			FirstController->Possess(ServerPawn.Get());
			LocalController->Possess(LocalPawn.Get());
			Advance();
			return false;
		}
		TArray<AInitNetworkEventPawn*> Copies;
		for (UWorld* World : Worlds)
		{
			for (TActorIterator<AInitNetworkEventPawn> It(World); It; ++It)
			{
				if (It->GetFName() == ServerPawn->GetFName())
				{
					Copies.Add(*It);
				}
			}
		}
		if (Copies.Num() != 3)
		{
			return false;
		}
		if (Phase == 2)
		{
			for (AInitNetworkEventPawn* Copy : Copies)
			{
				if (Copy->GetPawnInitializationComponent()->IsPawnInitialized())
				{
					return false;
				}
			}
			for (AInitNetworkEventPawn* Copy : Copies)
			{
				UInitNetworkEventComponent* Component = Copy->FindComponentByClass<UInitNetworkEventComponent>();
				Test.TestFalse(TEXT("Unpossession invalidates each peer generation"),
				               Before.FindChecked(Copy).Generation == Component->GetInitGeneration());
				Test.TestFalse(TEXT("Unpossession clears old player state bindings"),
				               Component->ReadyPlayerState.IsValid());
			}
			FirstController->Possess(ServerPawn.Get());
			Advance();
			return false;
		}
		for (AInitNetworkEventPawn* Copy : Copies)
		{
			if (Phase == 4 && (!Copy->GetPlayerState() ||
			                   Copy->GetPlayerState()->GetPlayerId() != ServerPawn->GetPlayerState()->GetPlayerId()))
			{
				return false;
			}
			if (!Copy->GetPawnInitializationComponent()->IsPawnInitialized())
			{
				return false;
			}
		}
		if (!LocalPawn->GetPawnInitializationComponent()->IsPawnInitialized())
		{
			return false;
		}
		bool bAuthority = false;
		bool bAutonomous = false;
		bool bSimulated = false;
		for (AInitNetworkEventPawn* Copy : Copies)
		{
			UInitNetworkEventComponent* Component = Copy->FindComponentByClass<UInitNetworkEventComponent>();
			Test.TestTrue(TEXT("Static configuration exists independently on every peer"), Component != nullptr);
			Test.TestFalse(TEXT("Configured component instances do not replicate"), Component->GetIsReplicated());
			Test.TestTrue(TEXT("Ready binding matches replicated player state"),
			              Component->ReadyPlayerState == Copy->GetPlayerState());
			bAuthority |= Copy->GetLocalRole() == ROLE_Authority;
			bAutonomous |= Copy->GetLocalRole() == ROLE_AutonomousProxy;
			bSimulated |= Copy->GetLocalRole() == ROLE_SimulatedProxy;
			if (Copy->GetLocalRole() == ROLE_AutonomousProxy)
			{
				Test.TestTrue(TEXT("Owning client initializes after input setup"), Component->bHadLocalInput);
				Test.TestTrue(TEXT("Owning client binds its controller"),
				              Component->ReadyController == Copy->GetController());
			}
			if (Phase > 1)
			{
				Test.TestFalse(TEXT("Context replacement creates new local generations"),
				               Before.FindChecked(Copy).Generation == Component->GetInitGeneration());
			}
			if (Phase == 4)
			{
				Test.TestFalse(TEXT("Controller migration replaces player state on each peer"),
				               Before.FindChecked(Copy).PlayerState == Component->ReadyPlayerState);
			}
		}
		Test.TestTrue(TEXT("Authority role covered"), bAuthority);
		Test.TestTrue(TEXT("Autonomous role covered"), bAutonomous);
		Test.TestTrue(TEXT("Simulated role covered"), bSimulated);
		Test.TestTrue(TEXT("Listen-server local player initializes after input setup"),
		              LocalPawn->FindComponentByClass<UInitNetworkEventComponent>()->bHadLocalInput);
		Before.Empty();
		for (AInitNetworkEventPawn* Copy : Copies)
		{
			UInitNetworkEventComponent* Component = Copy->FindComponentByClass<UInitNetworkEventComponent>();
			Before.Add(Copy, {Component->GetInitGeneration(), Component->ReadyPlayerState});
		}
		if (Phase == 1)
		{
			FirstController->UnPossess();
			Advance();
			return false;
		}
		if (Phase == 3)
		{
			SecondController->Possess(ServerPawn.Get());
			Advance();
			return false;
		}
		ServerPawn->Destroy();
		LocalPawn->Destroy();
		return true;
	}

private:
	struct FPeerSnapshot
	{
		FInitGeneration Generation;
		TWeakObjectPtr<APlayerState> PlayerState;
	};

	void Advance()
	{
		++Phase;
		StartedAt = FPlatformTime::Seconds();
	}

	FAutomationTestBase& Test;
	int32 Phase = 0;
	double StartedAt = FPlatformTime::Seconds();
	TWeakObjectPtr<APlayerController> FirstController;
	TWeakObjectPtr<APlayerController> SecondController;
	TWeakObjectPtr<AInitNetworkEventPawn> ServerPawn;
	TWeakObjectPtr<AInitNetworkEventPawn> LocalPawn;
	TMap<TWeakObjectPtr<AInitNetworkEventPawn>, FPeerSnapshot> Before;
#if DO_ENABLE_NET_TEST
	TMap<TWeakObjectPtr<UNetDriver>, FPacketSimulationSettings> SavedPacketSettings;
#endif
};

class FRestorePlaySettingsCommand final : public IAutomationLatentCommand
{
public:
	FRestorePlaySettingsCommand(EPlayNetMode InMode, bool bInOneProcess, int32 InClients, bool bInSeparateServer)
	    : Mode(InMode), bOneProcess(bInOneProcess), Clients(InClients), bSeparateServer(bInSeparateServer)
	{
	}

	virtual bool Update() override
	{
		if (!NetworkWorlds().IsEmpty())
		{
			return false;
		}
		ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
		Settings->SetPlayNetMode(Mode);
		Settings->SetRunUnderOneProcess(bOneProcess);
		Settings->SetPlayNumberOfClients(Clients);
		Settings->bLaunchSeparateServer = bSeparateServer;
		return true;
	}

private:
	EPlayNetMode Mode;
	bool bOneProcess;
	int32 Clients;
	bool bSeparateServer;
};
} // namespace Nelaric::Tests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPawnNetworkEventsTest, "Nelaric.GameplayRuntime.Pawn.NetworkEvents",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPawnNetworkEventsTest::RunTest(const FString& Parameters)
{
	if (!GEditor || GEditor->PlayWorld)
	{
		AddError(TEXT("NetworkEvents requires an editor with no active PIE session."));
		return false;
	}
	ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
	EPlayNetMode OldMode = PIE_Standalone;
	bool bOldOneProcess = true;
	int32 OldClients = 1;
	Settings->GetPlayNetMode(OldMode);
	Settings->GetRunUnderOneProcess(bOldOneProcess);
	Settings->GetPlayNumberOfClients(OldClients);
	const bool bOldSeparateServer = Settings->bLaunchSeparateServer;
	UWorld* EditorWorld = FAutomationEditorCommonUtils::CreateNewMap();
	EditorWorld->SpawnActor<APlayerStart>();
	Settings->SetPlayNetMode(PIE_ListenServer);
	Settings->SetRunUnderOneProcess(true);
	Settings->SetPlayNumberOfClients(3);
	Settings->bLaunchSeparateServer = false;
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(Nelaric::Tests::FNetworkContextCommand(*this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(
	    Nelaric::Tests::FRestorePlaySettingsCommand(OldMode, bOldOneProcess, OldClients, bOldSeparateServer));
	return true;
}

#endif
