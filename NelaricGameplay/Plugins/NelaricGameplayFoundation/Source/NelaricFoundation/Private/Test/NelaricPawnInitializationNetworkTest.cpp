// Copyright (c) 2026 Nelaric

#include "Test/NelaricPawnInitializationNetworkTestTypes.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Pawn/NelaricPawnInitializationComponent.h"
#include "Pawn/NelaricPawnInitializationConfig.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"

#endif

bool UNelaricInitNetworkTestC::CanEntryReady()
{
	const APawn* Pawn = GetPawn();
	const APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	return Controller && PlayerState && Controller->PlayerState == PlayerState;
}

void UNelaricInitNetworkTestC::OnInitReady()
{
	const APawn* Pawn = GetPawn();
	ReadyController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	ReadyPlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	bControllerAndPlayerStateUsableAfterReady = ReadyController.IsValid() && ReadyPlayerState.IsValid() &&
	                                            ReadyController->PlayerState == ReadyPlayerState.Get();
}

bool UNelaricInitNetworkTestC::TouchPlayerContext()
{
	if (GetInitState() != Nelaric::EInitState::Ready || !ReadyController.IsValid() || !ReadyPlayerState.IsValid() ||
	    ReadyController->PlayerState != ReadyPlayerState.Get())
	{
		return false;
	}
	++ReadyCallCount;
	return true;
}

void UNelaricInitNetworkTestD::OnInitReady()
{
	UNelaricInitNetworkTestA* A = FindObject<UNelaricInitNetworkTestA>(GetOwner(), TEXT("NelaricInit_A"));
	UNelaricInitNetworkTestB* B = FindObject<UNelaricInitNetworkTestB>(GetOwner(), TEXT("NelaricInit_B"));
	UNelaricInitNetworkTestC* C = FindObject<UNelaricInitNetworkTestC>(GetOwner(), TEXT("NelaricInit_C"));
	bAllDependenciesUsableAfterReady = A && B && C && A->GetInitState() == Nelaric::EInitState::Ready &&
	                                   B->GetInitState() == Nelaric::EInitState::Ready &&
	                                   C->GetInitState() == Nelaric::EInitState::Ready &&
	                                   A->bPeerCallSucceededAfterReady && B->bPeerCallSucceededAfterReady &&
	                                   C->bControllerAndPlayerStateUsableAfterReady && A->TouchPreparedObject() &&
	                                   B->TouchPreparedObject() && C->TouchPlayerContext();
}

void UNelaricInitNetworkTestGraphNode::OnInitReady()
{
	bReadyCallbackRan = true;
	UNelaricPawnInitializationComponent* Manager =
	    GetOwner()->FindComponentByClass<UNelaricPawnInitializationComponent>();
	if (!Manager || !Manager->InitializationConfig)
	{
		return;
	}
	for (const FNelaricPawnInitializationEntry& Entry : Manager->InitializationConfig->Components)
	{
		if (!Manager->IsConfiguredInstance(Entry.ComponentId, this))
		{
			continue;
		}
		bAllTargetCallsSucceeded = true;
		for (FName TargetId : Entry.DependencyIds)
		{
			const FString TargetName = TEXT("NelaricInit_") + TargetId.ToString();
			UActorComponent* Target = FindObject<UActorComponent>(GetOwner(), *TargetName);
			bool bCalled = false;
			if (Target && Manager->IsConfiguredInstance(TargetId, Target))
			{
				if (UNelaricInitNetworkTestGraphNode* Node = Cast<UNelaricInitNetworkTestGraphNode>(Target))
				{
					bCalled = Node->ReceiveReadyCall(Entry.ComponentId);
				}
				else if (UNelaricInitNetworkTestPeer* Peer = Cast<UNelaricInitNetworkTestPeer>(Target))
				{
					bCalled = Peer->TouchPreparedObject();
				}
				else if (UNelaricInitNetworkTestC* C = Cast<UNelaricInitNetworkTestC>(Target))
				{
					bCalled = C->TouchPlayerContext();
				}
				else if (UNelaricInitNetworkTestD* D = Cast<UNelaricInitNetworkTestD>(Target))
				{
					bCalled = D->TouchDependencies();
				}
			}
			if (bCalled)
			{
				CalledTargetIds.Add(TargetId);
			}
			bAllTargetCallsSucceeded &= bCalled;
		}
		return;
	}
}

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace
{
class FInitNetworkResult
{
public:
	bool bPassed = true;

	void AddError(const FString& Message)
	{
		bPassed = false;
		UE_LOG(LogTemp, Error, TEXT("Nelaric pawn init network test: %s"), *Message);
	}

	bool TestTrue(const TCHAR* Label, bool bValue)
	{
		if (!bValue)
		{
			AddError(FString(Label));
		}
		return bValue;
	}

	bool TestFalse(const TCHAR* Label, bool bValue)
	{
		return TestTrue(Label, !bValue);
	}

	template <typename T> bool TestNotNull(const TCHAR* Label, const T* Value)
	{
		return TestTrue(Label, Value != nullptr);
	}
};

struct FInitLatencyRange
{
	int32 MinMs;
	int32 MaxMs;
};

constexpr FInitLatencyRange LatencyRanges[] = {{0, 100}, {100, 500}, {500, 2000}, {2000, 10000}};

UWorld* FindNetworkWorld(ENetMode Mode)
{
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		UWorld* World = Context.World();
		if (Context.WorldType == EWorldType::PIE && World && World->GetNetMode() == Mode)
		{
			return World;
		}
	}
	return nullptr;
}

ANelaricInitNetworkTestPawn* FindTestPawn(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ANelaricInitNetworkTestPawn> It(World); It; ++It)
	{
		if (IsValid(*It))
		{
			return *It;
		}
	}
	return nullptr;
}

bool CheckPawnRound(FInitNetworkResult& Test, ANelaricInitNetworkTestPawn* Pawn, const FString& Label)
{
	if (!Test.TestNotNull(*FString::Printf(TEXT("%s pawn"), *Label), Pawn))
	{
		return false;
	}
	UNelaricPawnInitializationComponent* Manager = Pawn->FindComponentByClass<UNelaricPawnInitializationComponent>();
	UNelaricInitNetworkTestA* A = FindObject<UNelaricInitNetworkTestA>(Pawn, TEXT("NelaricInit_A"));
	UNelaricInitNetworkTestB* B = FindObject<UNelaricInitNetworkTestB>(Pawn, TEXT("NelaricInit_B"));
	UNelaricInitNetworkTestC* C = FindObject<UNelaricInitNetworkTestC>(Pawn, TEXT("NelaricInit_C"));
	UNelaricInitNetworkTestD* D = FindObject<UNelaricInitNetworkTestD>(Pawn, TEXT("NelaricInit_D"));
	if (!Test.TestNotNull(*FString::Printf(TEXT("%s manager"), *Label), Manager) ||
	    !Test.TestNotNull(*FString::Printf(TEXT("%s A"), *Label), A) ||
	    !Test.TestNotNull(*FString::Printf(TEXT("%s B"), *Label), B) ||
	    !Test.TestNotNull(*FString::Printf(TEXT("%s C"), *Label), C) ||
	    !Test.TestNotNull(*FString::Printf(TEXT("%s D"), *Label), D))
	{
		return false;
	}
	Test.TestTrue(*FString::Printf(TEXT("%s pawn Ready"), *Label), Manager->IsPawnInitialized());
	Test.TestTrue(*FString::Printf(TEXT("%s A sees B created before preparation"), *Label),
	              A->bPeerObjectExistedBeforePreparation);
	Test.TestTrue(*FString::Printf(TEXT("%s B sees A created before preparation"), *Label),
	              B->bPeerObjectExistedBeforePreparation);
	Test.TestTrue(*FString::Printf(TEXT("%s A calls B only after both Ready"), *Label),
	              A->bPeerCallSucceededAfterReady);
	Test.TestTrue(*FString::Printf(TEXT("%s B calls A only after both Ready"), *Label),
	              B->bPeerCallSucceededAfterReady);
	Test.TestTrue(*FString::Printf(TEXT("%s C obtains controller and replicated player state"), *Label),
	              C->bControllerAndPlayerStateUsableAfterReady);
	Test.TestTrue(*FString::Printf(TEXT("%s D uses A, B and C after Ready"), *Label),
	              D->bAllDependenciesUsableAfterReady);
	Test.TestTrue(*FString::Printf(TEXT("%s A received calls from B, D and Mixed"), *Label),
	              A->PreparedObjectCallCount == 3);
	Test.TestTrue(*FString::Printf(TEXT("%s B received calls from A, D and Mixed"), *Label),
	              B->PreparedObjectCallCount == 3);
	Test.TestTrue(*FString::Printf(TEXT("%s C received calls from D and Mixed"), *Label), C->ReadyCallCount == 2);
	Test.TestTrue(*FString::Printf(TEXT("%s D received a call from Mixed"), *Label), D->ReadyCallCount == 1);
	int32 CheckedEdges = 0;
	for (const FNelaricPawnInitializationEntry& Entry : Manager->InitializationConfig->Components)
	{
		if (Entry.ComponentClass != UNelaricInitNetworkTestGraphNode::StaticClass())
		{
			continue;
		}
		const FString Prefix = Label + TEXT(" ") + Entry.ComponentId.ToString();
		UNelaricInitNetworkTestGraphNode* Node =
		    FindObject<UNelaricInitNetworkTestGraphNode>(Pawn, *(TEXT("NelaricInit_") + Entry.ComponentId.ToString()));
		const bool bCreatedOnThisSide = Pawn->HasAuthority() ? Entry.bCreateOnAuthority : Entry.bCreateOnClient;
		if (!bCreatedOnThisSide)
		{
			Test.TestTrue(*(Prefix + TEXT(" absent on excluded network side")), Node == nullptr);
			continue;
		}
		if (!Test.TestNotNull(*(Prefix + TEXT(" created by manager")), Node))
		{
			continue;
		}
		Test.TestTrue(*(Prefix + TEXT(" managed instance")), Manager->IsConfiguredInstance(Entry.ComponentId, Node));
		Test.TestTrue(*(Prefix + TEXT(" Ready callback ran")), Node->bReadyCallbackRan);
		Test.TestTrue(*(Prefix + TEXT(" calls every dependency target successfully")), Node->bAllTargetCallsSucceeded);
		Test.TestTrue(*(Prefix + TEXT(" exactly one call per declared dependency")),
		              Node->CalledTargetIds.Num() == Entry.DependencyIds.Num());
		for (FName TargetId : Entry.DependencyIds)
		{
			Test.TestTrue(*(Prefix + TEXT(" called ") + TargetId.ToString()), Node->CalledTargetIds.Contains(TargetId));
			++CheckedEdges;
		}
		int32 ExpectedIncomingCalls = 0;
		for (const FNelaricPawnInitializationEntry& Caller : Manager->InitializationConfig->Components)
		{
			const bool bCallerCreated = Pawn->HasAuthority() ? Caller.bCreateOnAuthority : Caller.bCreateOnClient;
			if (bCallerCreated && Caller.DependencyIds.Contains(Entry.ComponentId))
			{
				++ExpectedIncomingCalls;
				Test.TestTrue(*(Prefix + TEXT(" received call from ") + Caller.ComponentId.ToString()),
				              Node->ReceivedCallerIds.Contains(Caller.ComponentId));
			}
		}
		Test.TestTrue(*(Prefix + TEXT(" exact incoming call count")),
		              Node->ReceivedCallerIds.Num() == ExpectedIncomingCalls);
	}
	UE_LOG(LogTemp, Display,
	       TEXT("Nelaric pawn init network test: %s checked %d graph dependency calls plus A/B/D calls."), *Label,
	       CheckedEdges);
	return Manager->IsPawnInitialized() && A->bPeerCallSucceededAfterReady && B->bPeerCallSucceededAfterReady &&
	       C->bControllerAndPlayerStateUsableAfterReady && D->bAllDependenciesUsableAfterReady;
}

class FInitNetworkRoundCommand final
{
public:
	FInitNetworkRoundCommand(FInitNetworkResult& InTest, UNelaricPawnInitializationConfig* InConfig)
	    : Test(InTest), Config(InConfig)
	{
	}

	bool Update()
	{
		UWorld* Server = FindNetworkWorld(NM_ListenServer);
		if (!Server)
		{
			Server = FindNetworkWorld(NM_DedicatedServer);
		}
		UWorld* Client = FindNetworkWorld(NM_Client);
		if (!Server || !Client || !Server->GetNetDriver() || !Client->GetNetDriver())
		{
			if (FPlatformTime::Seconds() - StartedAt > 30.0)
			{
				Test.AddError(TEXT("One-process network PIE did not establish a server and a client."));
				Cleanup(Server, Client);
				return true;
			}
			return false;
		}
		if (RangeIndex >= UE_ARRAY_COUNT(LatencyRanges))
		{
			Cleanup(Server, Client);
			return true;
		}
		if (PreviousClientPawn.IsValid())
		{
			if (FPlatformTime::Seconds() - StartedAt > 90.0)
			{
				Test.AddError(TEXT("The previous replicated pawn was not removed before the next latency range."));
				Cleanup(Server, Client);
				return true;
			}
			return false;
		}
		if (!bRoundStarted)
		{
			APlayerController* ServerController = nullptr;
			for (TActorIterator<APlayerController> It(Server); It; ++It)
			{
				if (It->GetNetConnection())
				{
					ServerController = *It;
					break;
				}
			}
			if (!ServerController || !ServerController->PlayerState)
			{
				if (FPlatformTime::Seconds() - StartedAt > 30.0)
				{
					Test.AddError(TEXT("Network PIE did not produce a connected player controller and player state."));
					Cleanup(Server, Client);
					return true;
				}
				return false;
			}
			if (!bSavedSettings)
			{
#if DO_ENABLE_NET_TEST
				ServerSettings = Server->GetNetDriver()->PacketSimulationSettings;
				ClientSettings = Client->GetNetDriver()->PacketSimulationSettings;
#endif
				bSavedSettings = true;
			}
#if DO_ENABLE_NET_TEST
			FPacketSimulationSettings Settings;
			Settings.PktLagMin = LatencyRanges[RangeIndex].MinMs;
			Settings.PktLagMax = LatencyRanges[RangeIndex].MaxMs;
			Server->GetNetDriver()->SetPacketSimulationSettings(Settings);
			Client->GetNetDriver()->SetPacketSimulationSettings(Settings);
			if (Server->GetNetDriver()->PacketSimulationSettings.PktLagMin != Settings.PktLagMin ||
			    Server->GetNetDriver()->PacketSimulationSettings.PktLagMax != Settings.PktLagMax ||
			    Client->GetNetDriver()->PacketSimulationSettings.PktLagMin != Settings.PktLagMin ||
			    Client->GetNetDriver()->PacketSimulationSettings.PktLagMax != Settings.PktLagMax)
			{
				Test.AddError(TEXT("The requested packet lag range was not applied to both net drivers."));
				Cleanup(Server, Client);
				return true;
			}
#else
			Test.AddError(TEXT("UE packet simulation is disabled in this build."));
			Cleanup(Server, Client);
			return true;
#endif
			ServerPawn = Server->SpawnActor<ANelaricInitNetworkTestPawn>();
			if (!ServerPawn.IsValid())
			{
				Test.AddError(TEXT("Failed to spawn configured replicated pawn."));
				Cleanup(Server, Client);
				return true;
			}
			ServerController->Possess(ServerPawn.Get());
			if (UNelaricPawnInitializationComponent* Manager =
			        ServerPawn->FindComponentByClass<UNelaricPawnInitializationComponent>())
			{
				Manager->SetInitializationConfig(Config.Get());
			}
			bRoundStarted = true;
			StartedAt = FPlatformTime::Seconds();
			UE_LOG(LogTemp, Display, TEXT("Nelaric pawn init network test: testing %d-%d ms packet lag."),
			       LatencyRanges[RangeIndex].MinMs, LatencyRanges[RangeIndex].MaxMs);
			return false;
		}
		ANelaricInitNetworkTestPawn* ClientPawn = FindTestPawn(Client);
		if (ClientPawn)
		{
			if (UNelaricPawnInitializationComponent* Manager =
			        ClientPawn->FindComponentByClass<UNelaricPawnInitializationComponent>())
			{
				Manager->SetInitializationConfig(Config.Get());
			}
			if (UNelaricInitNetworkTestC* C = FindObject<UNelaricInitNetworkTestC>(ClientPawn, TEXT("NelaricInit_C")))
			{
				const APlayerController* Controller = Cast<APlayerController>(ClientPawn->GetController());
				const bool bPlayerContextAvailable = Controller && ClientPawn->GetPlayerState() &&
				                                     Controller->PlayerState == ClientPawn->GetPlayerState();
				if (!bCheckedPendingClientContext && !bPlayerContextAvailable)
				{
					Test.TestFalse(TEXT("C cannot be Ready before replicated player context arrives"),
					               C->GetInitState() == Nelaric::EInitState::Ready);
					bCheckedPendingClientContext = true;
				}
				C->RequestInitRefresh();
			}
		}
		UNelaricPawnInitializationComponent* ClientManager =
		    ClientPawn ? ClientPawn->FindComponentByClass<UNelaricPawnInitializationComponent>() : nullptr;
		UNelaricPawnInitializationComponent* ServerManager =
		    ServerPawn->FindComponentByClass<UNelaricPawnInitializationComponent>();
		if (ClientManager && ServerManager && ClientManager->IsPawnInitialized() && ServerManager->IsPawnInitialized())
		{
			const FString Label =
			    FString::Printf(TEXT("%d-%d ms"), LatencyRanges[RangeIndex].MinMs, LatencyRanges[RangeIndex].MaxMs);
			CheckPawnRound(Test, ServerPawn.Get(), Label + TEXT(" server"));
			CheckPawnRound(Test, ClientPawn, Label + TEXT(" client"));
			UE_LOG(LogTemp, Display, TEXT("Nelaric pawn init network test: %s completed in %.2f seconds."), *Label,
			       FPlatformTime::Seconds() - StartedAt);
			PreviousClientPawn = ClientPawn;
			ServerPawn->Destroy();
			ServerPawn.Reset();
			bRoundStarted = false;
			bCheckedPendingClientContext = false;
			++RangeIndex;
			StartedAt = FPlatformTime::Seconds();
			return false;
		}
		if (FPlatformTime::Seconds() - StartedAt > 90.0)
		{
			Test.AddError(FString::Printf(TEXT("Pawn did not become Ready under %d-%d ms packet lag."),
			                              LatencyRanges[RangeIndex].MinMs, LatencyRanges[RangeIndex].MaxMs));
			Cleanup(Server, Client);
			return true;
		}
		return false;
	}

private:
	void Cleanup(UWorld* Server, UWorld* Client)
	{
		if (ServerPawn.IsValid())
		{
			ServerPawn->Destroy();
		}
#if DO_ENABLE_NET_TEST
		if (bSavedSettings)
		{
			if (Server && Server->GetNetDriver())
			{
				Server->GetNetDriver()->SetPacketSimulationSettings(ServerSettings);
			}
			if (Client && Client->GetNetDriver())
			{
				Client->GetNetDriver()->SetPacketSimulationSettings(ClientSettings);
			}
		}
#endif
		Config.Reset();
	}

	FInitNetworkResult& Test;
	TStrongObjectPtr<UNelaricPawnInitializationConfig> Config;
	TWeakObjectPtr<ANelaricInitNetworkTestPawn> ServerPawn;
	TWeakObjectPtr<ANelaricInitNetworkTestPawn> PreviousClientPawn;
	double StartedAt = FPlatformTime::Seconds();
	int32 RangeIndex = 0;
	bool bRoundStarted = false;
	bool bSavedSettings = false;
	bool bCheckedPendingClientContext = false;
#if DO_ENABLE_NET_TEST
	FPacketSimulationSettings ServerSettings;
	FPacketSimulationSettings ClientSettings;
#endif
};

void RunConfiguredNetworkInitializationTest()
{
	UNelaricPawnInitializationConfig* Config = NewObject<UNelaricPawnInitializationConfig>(GetTransientPackage());
	auto AddEntry = [Config](FName Id, UClass* Class, TArray<FName> Dependencies)
	{
		FNelaricPawnInitializationEntry Entry;
		Entry.ComponentId = Id;
		Entry.ComponentClass = Class;
		Entry.DependencyIds = MoveTemp(Dependencies);
		Config->Components.Add(MoveTemp(Entry));
	};
	AddEntry(TEXT("A"), UNelaricInitNetworkTestA::StaticClass(), {TEXT("B")});
	AddEntry(TEXT("B"), UNelaricInitNetworkTestB::StaticClass(), {TEXT("A")});
	AddEntry(TEXT("C"), UNelaricInitNetworkTestC::StaticClass(), {});
	AddEntry(TEXT("D"), UNelaricInitNetworkTestD::StaticClass(), {TEXT("A"), TEXT("B"), TEXT("C")});
	auto AddGraphEntry = [Config, &AddEntry](FName Id, TArray<FName> Dependencies, bool bAuthority = true,
	                                         bool bClient = true, bool bRequired = true)
	{
		AddEntry(Id, UNelaricInitNetworkTestGraphNode::StaticClass(), MoveTemp(Dependencies));
		FNelaricPawnInitializationEntry& Entry = Config->Components.Last();
		Entry.bCreateOnAuthority = bAuthority;
		Entry.bCreateOnClient = bClient;
		Entry.bRequiredForPawnReady = bRequired;
	};
	AddGraphEntry(TEXT("Root"), {});
	AddGraphEntry(TEXT("Single"), {TEXT("Root")});
	AddGraphEntry(TEXT("ChainTail"), {TEXT("Single")});
	AddGraphEntry(TEXT("FanIn"), {TEXT("Root"), TEXT("Single")});
	AddGraphEntry(TEXT("FanOutLeft"), {TEXT("Root")});
	AddGraphEntry(TEXT("FanOutRight"), {TEXT("Root")});
	AddGraphEntry(TEXT("Diamond"), {TEXT("FanOutLeft"), TEXT("FanOutRight")});
	AddGraphEntry(TEXT("Self"), {TEXT("Self")});
	AddGraphEntry(TEXT("Cycle1"), {TEXT("Cycle2")});
	AddGraphEntry(TEXT("Cycle2"), {TEXT("Cycle3")});
	AddGraphEntry(TEXT("Cycle3"), {TEXT("Cycle1")});
	AddGraphEntry(TEXT("CycleDependent"), {TEXT("Cycle1")});
	AddGraphEntry(TEXT("CycleWithExternal1"), {TEXT("CycleWithExternal2"), TEXT("Diamond")});
	AddGraphEntry(TEXT("CycleWithExternal2"), {TEXT("CycleWithExternal1")});
	AddGraphEntry(TEXT("CycleDownstream"), {TEXT("CycleDependent"), TEXT("CycleWithExternal1")});
	AddGraphEntry(TEXT("OptionalTarget"), {}, true, true, false);
	AddGraphEntry(TEXT("RequiredFromOptional"), {TEXT("OptionalTarget")});
	AddGraphEntry(TEXT("OptionalFromRequired"), {TEXT("Single")}, true, true, false);
	AddGraphEntry(TEXT("OptionalFromOptional"), {TEXT("OptionalTarget")}, true, true, false);
	AddGraphEntry(TEXT("AuthorityTarget"), {}, true, false);
	AddGraphEntry(TEXT("AuthorityConsumer"), {TEXT("AuthorityTarget"), TEXT("Root")}, true, false);
	AddGraphEntry(TEXT("ClientTarget"), {}, false, true);
	AddGraphEntry(TEXT("ClientConsumer"), {TEXT("ClientTarget"), TEXT("Root")}, false, true);
	AddGraphEntry(TEXT("Mixed"), {TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D")});
	TSharedRef<FInitNetworkResult> Result = MakeShared<FInitNetworkResult>();
	TSharedRef<FInitNetworkRoundCommand> Round = MakeShared<FInitNetworkRoundCommand>(*Result, Config);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
	    [Result, Round](float)
	    {
		    if (!Round->Update())
		    {
			    return true;
		    }
		    if (Result->bPassed)
		    {
			    UE_LOG(LogTemp, Display, TEXT("Nelaric pawn init network test PASSED: all four packet-lag ranges."));
		    }
		    else
		    {
			    UE_LOG(LogTemp, Error, TEXT("Nelaric pawn init network test FAILED."));
		    }
		    return false;
	    }));
}

FAutoConsoleCommand GRunConfiguredNetworkInitializationTest(
    TEXT("Nelaric.TestPawnInitNetwork"),
    TEXT("Run configured pawn initialization in an existing one-process server/client PIE session."),
    FConsoleCommandDelegate::CreateStatic(&RunConfiguredNetworkInitializationTest));
} // namespace

#endif
