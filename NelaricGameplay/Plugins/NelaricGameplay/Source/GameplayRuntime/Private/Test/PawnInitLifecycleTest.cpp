// Copyright (c) 2026 Nelaric

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Test/PawnInitLifecycleTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPawnInitLifecycleGateTest, "Nelaric.GameplayRuntime.Pawn.InitLifecycleGates",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPawnInitLifecycleGateTest::RunTest(const FString& Parameters)
{
	UInitLifecycleDefaultTestComponent* Defaults = NewObject<UInitLifecycleDefaultTestComponent>();
	TestTrue(TEXT("Registered preparation defaults to true"), Defaults->TryChangeInitState());
	TestTrue(TEXT("Available data preparation defaults to true"), Defaults->TryChangeInitState());
	TestTrue(TEXT("Initialized data preparation defaults to true"), Defaults->CanEnterReady());
	UInitLifecycleGateTestComponent* Component = NewObject<UInitLifecycleGateTestComponent>();
	TestTrue(TEXT("Four lifecycle overrides suffice to participate"), Component->IsInitApplicable());
	TestFalse(TEXT("Registered work can remain pending"), Component->TryChangeInitState());
	TestEqual(TEXT("Pending Registered work keeps its state"), Component->GetInitState(),
	          Nelaric::EInitState::Registered);
	Component->bDataAvailable = true;
	TestTrue(TEXT("Registered work permits DataAvailable"), Component->TryChangeInitState());
	TestEqual(TEXT("Registered preparation retries until complete"), Component->RegisteredCalls, 2);
	TestFalse(TEXT("DataAvailable work can remain pending"), Component->TryChangeInitState());
	Component->bDataInitialized = true;
	TestTrue(TEXT("DataAvailable work permits DataInitialized"), Component->TryChangeInitState());
	TestEqual(TEXT("DataAvailable preparation retries until complete"), Component->AvailableCalls, 2);
	TestFalse(TEXT("Final preparation can remain pending"), Component->CanEnterReady());
	Component->bReadyPrepared = true;
	TestTrue(TEXT("Final preparation permits Ready"), Component->CanEnterReady());
	TestTrue(TEXT("Completed preparation remains valid"), Component->CanEnterReady());
	TestEqual(TEXT("Completed final work is not repeated"), Component->InitializedCalls, 2);
	const Nelaric::FInitStateSnapshot Previous{Component->GetInitGeneration(), Component->GetInitState(), false};
	TestTrue(TEXT("Coordinator commits Ready silently"), Component->CommitReadyWithoutNotification());
	TestEqual(TEXT("Ready gameplay waits for publication"), Component->ReadyCalls, 0);
	Component->NotifyReadyCommitted(Previous);
	Component->NotifyReadyCommitted(Previous);
	TestEqual(TEXT("Ready gameplay runs once"), Component->ReadyCalls, 1);
	Component->bReadyPrepared = false;
	Component->InvalidateInitGeneration();
	TestTrue(TEXT("New generation prepares Registered again"), Component->TryChangeInitState());
	TestTrue(TEXT("New generation prepares available data again"), Component->TryChangeInitState());
	TestFalse(TEXT("Invalidation clears completed final preparation"), Component->CanEnterReady());
	TestEqual(TEXT("Final work is invoked in the new generation"), Component->InitializedCalls, 3);
	return true;
}

#endif
