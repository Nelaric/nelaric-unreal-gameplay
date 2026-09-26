// Copyright (c) 2026 Nelaric

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Session/NelaricSessionTransitionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Internal/InternalAccess.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNelaricSessionTransitionTest, "Nelaric.Foundation.Session.ClientToClientApproval",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNelaricSessionTransitionTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone(FName(TEXT("/Temp/NelaricTransitionTest")), nullptr);
	UWorld* InitialWorld = GameInstance->GetWorld();
	UWorld* ClientWorld =
	    UWorld::CreateWorld(EWorldType::PIE, false, FName(TEXT("NelaricTransitionClientTest")), nullptr, false);
	ClientWorld->SetPlayInEditorInitialNetMode(NM_Client);
	ClientWorld->SetGameInstance(GameInstance);
	GameInstance->GetWorldContext()->SetCurrentWorld(ClientWorld);

	UNelaricSessionTransitionSubsystem* Coordinator = GameInstance->GetSubsystem<UNelaricSessionTransitionSubsystem>();
	const UE::Nelaric::FInternalAccessKey& Key = UE::Nelaric::FInternalAccess::Key();
	int32 SourceStarts = 0;
	int32 TargetStarts = 0;
	int32 Terminations = 0;
	int32 Failures = 0;
	int32 Cancellations = 0;
	UE::Nelaric::ETransitionError LastError = UE::Nelaric::ETransitionError::UnexpectedNetMode;
	UE::Nelaric::FTransitionHandle LastTerminalHandle;

	UE::Nelaric::FStartTransitionApproval Source = UE::Nelaric::FStartTransitionApproval::CreateLambda(
	    [&SourceStarts](uint64, const UE::Nelaric::FTransitionDestination&)
	    {
		    ++SourceStarts;
		    return true;
	    });
	UE::Nelaric::FStartTransitionApproval Target = UE::Nelaric::FStartTransitionApproval::CreateLambda(
	    [&TargetStarts](uint64, const UE::Nelaric::FTransitionDestination&)
	    {
		    ++TargetStarts;
		    return true;
	    });
	UE::Nelaric::FOnTransitionTerminated Terminated =
	    UE::Nelaric::FOnTransitionTerminated::CreateLambda([&Terminations](uint64) { ++Terminations; });
	Coordinator->InternalConfigureApprovalTransport(Key, Source, Target, Terminated);

	UE::Nelaric::FTransitionDestination Destination;
	Destination.GameEndpoint = {TEXT("127.0.0.1"), 7777};
	Destination.BeaconEndpoint = {TEXT("127.0.0.1"), 15000};
	UE::Nelaric::FTransitionCallbacks Callbacks;
	Callbacks.OnFailed.BindLambda(
	    [&](UE::Nelaric::FTransitionHandle Handle, UE::Nelaric::ETransitionError Error)
	    {
		    ++Failures;
		    LastTerminalHandle = Handle;
		    LastError = Error;
	    });
	Callbacks.OnCancelled.BindLambda(
	    [&](UE::Nelaric::FTransitionHandle Handle)
	    {
		    ++Cancellations;
		    LastTerminalHandle = Handle;
	    });

	TestTrue(TEXT("Synthetic world reports client mode"),
	         Coordinator->GetNetMode().IsSet() && Coordinator->GetNetMode().GetValue() == NM_Client);
	UE::Nelaric::FTransitionDestination InvalidDestination = Destination;
	InvalidDestination.BeaconEndpoint.Port = 0;
	TestEqual(TEXT("Missing beacon endpoint is rejected"),
	          Coordinator->RequestTransition(NM_Client, InvalidDestination, Callbacks).Id, uint64(0));
	TestEqual(TEXT("Unsupported destination mode is rejected"),
	          Coordinator->RequestTransition(NM_ListenServer, Destination, Callbacks).Id, uint64(0));
	TestEqual(TEXT("Rejected requests do not start approval"), SourceStarts + TargetStarts, 0);

	const UE::Nelaric::FTransitionHandle First = Coordinator->RequestTransition(NM_Client, Destination, Callbacks);
	TestTrue(TEXT("Valid request is accepted"), First.Id != 0);
	TestEqual(TEXT("Source approval starts once"), SourceStarts, 1);
	TestEqual(TEXT("Target approval starts once"), TargetStarts, 1);
	TestEqual(TEXT("Concurrent request is rejected"),
	          Coordinator->RequestTransition(NM_Client, Destination, Callbacks).Id, uint64(0));
	Coordinator->InternalReportSourceApproval(Key, First.Id + 1, false);
	TestEqual(TEXT("Unrelated decision does not finish request"), Failures, 0);
	Coordinator->InternalReportSourceApproval(Key, First.Id, false);
	TestEqual(TEXT("Source denial fails exactly once"), Failures, 1);
	TestEqual(TEXT("Source denial error"), LastError, UE::Nelaric::ETransitionError::AuthorityRejected);
	TestEqual(TEXT("Source denial keeps request identity"), LastTerminalHandle.Id, First.Id);
	TestEqual(TEXT("Source denial releases transport"), Terminations, 1);
	Coordinator->InternalReportTargetApproval(Key, First.Id, true);
	TestEqual(TEXT("Late decision cannot finish again"), Failures, 1);

	const UE::Nelaric::FTransitionHandle Second = Coordinator->RequestTransition(NM_Client, Destination, Callbacks);
	TestTrue(TEXT("Request can restart after denial"), Second.Id > First.Id);
	Coordinator->InternalReportSourceApproval(Key, Second.Id, true);
	Coordinator->InternalReportTargetApproval(Key, Second.Id, true);
	TestTrue(TEXT("Approved request can be cancelled before travel"), Coordinator->CancelTransition(Second));
	TestEqual(TEXT("Cancellation callback runs once"), Cancellations, 1);
	TestEqual(TEXT("Cancellation keeps request identity"), LastTerminalHandle.Id, Second.Id);
	TestEqual(TEXT("Cancellation releases transport"), Terminations, 2);
	TestFalse(TEXT("Finished request cannot be cancelled again"), Coordinator->CancelTransition(Second));

	const UE::Nelaric::FTransitionHandle Third = Coordinator->RequestTransition(NM_Client, Destination, Callbacks);
	Coordinator->InternalReportTargetUnavailable(Key, Third.Id);
	TestEqual(TEXT("Unavailable target fails"), Failures, 2);
	TestEqual(TEXT("Unavailable target error"), LastError, UE::Nelaric::ETransitionError::AuthorityUnavailable);
	TestEqual(TEXT("Unavailable target releases transport"), Terminations, 3);

	Coordinator->InternalClearApprovalTransport(Key);
	GameInstance->GetWorldContext()->SetCurrentWorld(InitialWorld);
	ClientWorld->DestroyWorld(false);
	GameInstance->Shutdown();
	GEngine->DestroyWorldContext(InitialWorld);
	InitialWorld->DestroyWorld(false);
	InitialWorld->RemoveFromRoot();
	GameInstance->RemoveFromRoot();
	return true;
}

#endif
