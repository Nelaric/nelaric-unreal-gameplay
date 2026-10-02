// Copyright (c) 2026 Nelaric Contributors

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Session/SessionTransitionSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Internal/GameplayRuntimeInternalAccess.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionTransitionTest, "Nelaric.GameplayRuntime.Session.ClientToClientApproval",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionTransitionTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone(FName(TEXT("/Temp/TransitionTest")), nullptr);
	UWorld* InitialWorld = GameInstance->GetWorld();
	UWorld* ClientWorld =
	    UWorld::CreateWorld(EWorldType::PIE, false, FName(TEXT("TransitionClientTest")), nullptr, false);
	ClientWorld->SetPlayInEditorInitialNetMode(NM_Client);
	ClientWorld->SetGameInstance(GameInstance);
	GameInstance->GetWorldContext()->SetCurrentWorld(ClientWorld);

	USessionTransitionSubsystem* Coordinator = GameInstance->GetSubsystem<USessionTransitionSubsystem>();
	const Nelaric::FGameplayRuntimeInternalAccessKey& Key = Nelaric::FGameplayRuntimeInternalAccess::Key();
	int32 SourceStarts = 0;
	int32 TargetStarts = 0;
	int32 Terminations = 0;
	int32 Failures = 0;
	int32 Cancellations = 0;
	Nelaric::ETransitionError LastError = Nelaric::ETransitionError::UnexpectedNetMode;
	Nelaric::FTransitionHandle LastTerminalHandle;

	Nelaric::FStartTransitionApproval Source = Nelaric::FStartTransitionApproval::CreateLambda(
	    [&SourceStarts](uint64, const Nelaric::FTransitionDestination&)
	    {
		    ++SourceStarts;
		    return true;
	    });
	Nelaric::FStartTransitionApproval Target = Nelaric::FStartTransitionApproval::CreateLambda(
	    [&TargetStarts](uint64, const Nelaric::FTransitionDestination&)
	    {
		    ++TargetStarts;
		    return true;
	    });
	Nelaric::FOnTransitionTerminated Terminated =
	    Nelaric::FOnTransitionTerminated::CreateLambda([&Terminations](uint64) { ++Terminations; });
	Coordinator->InternalConfigureApprovalTransport(Key, Source, Target, Terminated);

	Nelaric::FTransitionDestination Destination;
	Destination.GameEndpoint = {TEXT("127.0.0.1"), 7777};
	Destination.BeaconEndpoint = {TEXT("127.0.0.1"), 15000};
	Nelaric::FTransitionCallbacks Callbacks;
	Callbacks.OnFailed.BindLambda(
	    [&](Nelaric::FTransitionHandle Handle, Nelaric::ETransitionError Error)
	    {
		    ++Failures;
		    LastTerminalHandle = Handle;
		    LastError = Error;
	    });
	Callbacks.OnCancelled.BindLambda(
	    [&](Nelaric::FTransitionHandle Handle)
	    {
		    ++Cancellations;
		    LastTerminalHandle = Handle;
	    });

	TestTrue(TEXT("Synthetic world reports client mode"),
	         Coordinator->GetNetMode().IsSet() && Coordinator->GetNetMode().GetValue() == NM_Client);
	Nelaric::FTransitionDestination InvalidDestination = Destination;
	InvalidDestination.BeaconEndpoint.Port = 0;
	AddExpectedErrorPlain(
	    FString::Printf(TEXT("Transition request rejected: world=%s sourceMode=%d targetMode=%d gameEndpointValid=1 "
	                         "beaconEndpointValid=0 sourceTransportBound=1 targetTransportBound=1."),
	                    *ClientWorld->GetName(), static_cast<int32>(NM_Client), static_cast<int32>(NM_Client)),
	    EAutomationExpectedErrorFlags::Exact, 1);
	TestEqual(TEXT("Missing beacon endpoint is rejected"),
	          Coordinator->RequestTransition(NM_Client, InvalidDestination, Callbacks).Id, uint64(0));
	AddExpectedErrorPlain(
	    FString::Printf(TEXT("Transition request rejected: world=%s sourceMode=%d targetMode=%d gameEndpointValid=1 "
	                         "beaconEndpointValid=1 sourceTransportBound=1 targetTransportBound=1."),
	                    *ClientWorld->GetName(), static_cast<int32>(NM_Client), static_cast<int32>(NM_ListenServer)),
	    EAutomationExpectedErrorFlags::Exact, 1);
	TestEqual(TEXT("Unsupported destination mode is rejected"),
	          Coordinator->RequestTransition(NM_ListenServer, Destination, Callbacks).Id, uint64(0));
	TestEqual(TEXT("Rejected requests do not start approval"), SourceStarts + TargetStarts, 0);

	const Nelaric::FTransitionHandle First = Coordinator->RequestTransition(NM_Client, Destination, Callbacks);
	TestTrue(TEXT("Valid request is accepted"), First.Id != 0);
	TestEqual(TEXT("Source approval starts once"), SourceStarts, 1);
	TestEqual(TEXT("Target approval starts once"), TargetStarts, 1);
	AddExpectedErrorPlain(
	    FString::Printf(TEXT("Transition request rejected: another request is active (request=%llu)."),
	                    static_cast<unsigned long long>(First.Id)),
	    EAutomationExpectedErrorFlags::Exact, 1);
	TestEqual(TEXT("Concurrent request is rejected"),
	          Coordinator->RequestTransition(NM_Client, Destination, Callbacks).Id, uint64(0));
	Coordinator->InternalReportSourceApproval(Key, First.Id + 1, false);
	TestEqual(TEXT("Unrelated decision does not finish request"), Failures, 0);
	AddExpectedErrorPlain(
	    FString::Printf(TEXT("Transition failed: request=%llu error=%d sourceApproved=0 targetApproved=0 "
	                         "travelStarted=0."),
	                    static_cast<unsigned long long>(First.Id),
	                    static_cast<int32>(Nelaric::ETransitionError::AuthorityRejected)),
	    EAutomationExpectedErrorFlags::Exact, 1);
	Coordinator->InternalReportSourceApproval(Key, First.Id, false);
	TestEqual(TEXT("Source denial fails exactly once"), Failures, 1);
	TestEqual(TEXT("Source denial error"), LastError, Nelaric::ETransitionError::AuthorityRejected);
	TestEqual(TEXT("Source denial keeps request identity"), LastTerminalHandle.Id, First.Id);
	TestEqual(TEXT("Source denial releases transport"), Terminations, 1);
	Coordinator->InternalReportTargetApproval(Key, First.Id, true);
	TestEqual(TEXT("Late decision cannot finish again"), Failures, 1);

	const Nelaric::FTransitionHandle Second = Coordinator->RequestTransition(NM_Client, Destination, Callbacks);
	TestTrue(TEXT("Request can restart after denial"), Second.Id > First.Id);
	Coordinator->InternalReportSourceApproval(Key, Second.Id, true);
	Coordinator->InternalReportTargetApproval(Key, Second.Id, true);
	TestTrue(TEXT("Approved request can be cancelled before travel"), Coordinator->CancelTransition(Second));
	TestEqual(TEXT("Cancellation callback runs once"), Cancellations, 1);
	TestEqual(TEXT("Cancellation keeps request identity"), LastTerminalHandle.Id, Second.Id);
	TestEqual(TEXT("Cancellation releases transport"), Terminations, 2);
	TestFalse(TEXT("Finished request cannot be cancelled again"), Coordinator->CancelTransition(Second));

	const Nelaric::FTransitionHandle Third = Coordinator->RequestTransition(NM_Client, Destination, Callbacks);
	AddExpectedErrorPlain(
	    FString::Printf(TEXT("Transition failed: request=%llu error=%d sourceApproved=0 targetApproved=0 "
	                         "travelStarted=0."),
	                    static_cast<unsigned long long>(Third.Id),
	                    static_cast<int32>(Nelaric::ETransitionError::AuthorityUnavailable)),
	    EAutomationExpectedErrorFlags::Exact, 1);
	Coordinator->InternalReportTargetUnavailable(Key, Third.Id);
	TestEqual(TEXT("Unavailable target fails"), Failures, 2);
	TestEqual(TEXT("Unavailable target error"), LastError, Nelaric::ETransitionError::AuthorityUnavailable);
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
