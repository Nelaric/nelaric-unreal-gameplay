// Copyright (c) 2026 Nelaric

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Test/NelaricInitStateTestTypes.h"
#include "World/NelaricInitStateWorldSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNelaricInitStateContractTest, "Nelaric.Foundation.World.InitStateContract",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNelaricInitStateContractTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	UNelaricInitStateWorldSubsystem* Subsystem = World->GetSubsystem<UNelaricInitStateWorldSubsystem>();
	TestNotNull(TEXT("World owns coordinator"), Subsystem);
	APawn* Pawn = World->SpawnActor<APawn>();
	UNelaricInitStateTestPawnComponent* Standalone = NewObject<UNelaricInitStateTestPawnComponent>(Pawn);
	Standalone->bAllowAdvance = true;
	TestTrue(TEXT("TryChange commits one adjacent step"), Standalone->TryChangeInitState());
	TestEqual(TEXT("One call advances one stage"), Standalone->GetInitState(), Nelaric::EInitState::DataAvailable);
	TestFalse(TEXT("Skipped state is rejected"), Standalone->TestCommit(Nelaric::EInitState::Ready));
	TestTrue(TEXT("Preparation advances to DataInitialized"), Standalone->TryChangeInitState());
	TestFalse(TEXT("TryChange never enters Ready"), Standalone->TryChangeInitState());
	TestFalse(TEXT("Internal preparation gates Ready"), Standalone->CanEnterReady());
	TestFalse(TEXT("Direct Ready commit respects the gate"), Standalone->TestCommit(Nelaric::EInitState::Ready));
	UNelaricInitStateTestPawnComponent* First = NewObject<UNelaricInitStateTestPawnComponent>(Pawn);
	UNelaricInitStateTestPawnComponent* Dependent = NewObject<UNelaricInitStateTestPawnComponent>(Pawn);
	First->bAllowAdvance = true;
	Dependent->bAllowAdvance = true;
	Dependent->bInternalReady = true;
	Dependent->bDeclareDependency = true;
	First->RegisterComponent();
	Dependent->RegisterComponent();
	TestEqual(TEXT("Own preparation proceeds without a ready dependency"), Dependent->GetInitState(),
	          Nelaric::EInitState::DataInitialized);
	TestEqual(TEXT("Internal preparation blocks Ready"), First->GetInitState(), Nelaric::EInitState::DataInitialized);
	TestFalse(TEXT("Unresolved dependency fails the pure gate"), Dependent->CanEnterReady());
	Dependent->SetDependency(First);
	TestEqual(TEXT("Referenced dependency still waits for Ready"), Dependent->GetInitState(),
	          Nelaric::EInitState::DataInitialized);
	Dependent->SetDependency(nullptr);

	First->bInternalReady = true;
	First->RequestInitRefresh();
	TestEqual(TEXT("Dependency reaches ready"), First->GetInitState(), Nelaric::EInitState::Ready);
	TestEqual(TEXT("Missing reference remains blocked"), Dependent->GetInitState(),
	          Nelaric::EInitState::DataInitialized);
	Dependent->SetDependency(First);
	TestEqual(TEXT("Reference refresh unlocks Ready"), Dependent->GetInitState(), Nelaric::EInitState::Ready);
	TestFalse(TEXT("Ready cannot advance"), First->TryChangeInitState());

	First->MarkTerminalInitFailure();
	TestTrue(TEXT("Failure flag is component-owned"), First->HasTerminalInitFailure());
	const Nelaric::FInitGeneration OldGeneration = First->GetInitGeneration();
	TestFalse(TEXT("Failed attempt rejects asynchronous result"), First->CanApplyInitResult(World, OldGeneration));
	First->bAllowAdvance = false;
	First->InvalidateInitGeneration();
	TestFalse(TEXT("Invalidation clears failure"), First->HasTerminalInitFailure());
	TestEqual(TEXT("Invalidation resets stage"), First->GetInitState(), Nelaric::EInitState::Registered);
	TestTrue(TEXT("Invalidation changes generation"), !(OldGeneration == First->GetInitGeneration()));
	TestFalse(TEXT("Old asynchronous result is rejected"), First->CanApplyInitResult(World, OldGeneration));
	TestTrue(TEXT("New asynchronous result is accepted"), First->CanApplyInitResult(World, First->GetInitGeneration()));
	TestEqual(TEXT("Invalidation cancels old work"), First->CancelCount, 1);

	UActorComponent* Direct = NewObject<UNelaricInitStateTestDirectComponent>(Pawn);
	Direct->RegisterComponent();
	TestTrue(TEXT("UE reflection discovers direct participant"),
	         Direct->GetClass()->ImplementsInterface(UNelaricInitStateParticipantInterface::StaticClass()));
	TestNotNull(TEXT("Direct participant can be cast from UActorComponent"),
	            Cast<INelaricInitStateParticipantInterface>(Direct));
	Subsystem->RegisterParticipant(Direct);
	Subsystem->UnregisterParticipant(Direct);
	return true;
}

#endif
