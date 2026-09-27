// Copyright (c) 2026 Nelaric

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Pawn/NelaricPawnInitializationComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Pawn/NelaricCharacter.h"
#include "Pawn/NelaricPawn.h"
#include "Pawn/NelaricPawnInitializationHelper.h"
#include "Test/NelaricPawnInitializationTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNelaricPawnInitializationTest, "Nelaric.Foundation.Pawn.Initialization",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNelaricPawnInitializationTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	ANelaricPawn* Pawn = World->SpawnActor<ANelaricPawn>();
	ANelaricCharacter* Character = World->SpawnActor<ANelaricCharacter>();
	TestNotNull(TEXT("Pawn owns its initialization component"), Nelaric::Pawn::FInitializationHelper::Get(Pawn));
	TestNotNull(TEXT("Character owns its initialization component"),
	            Nelaric::Pawn::FInitializationHelper::Get(Character));
	UNelaricPawnInitializationTestComponent* Component = NewObject<UNelaricPawnInitializationTestComponent>(Pawn);
	Component->OnPawnInitialized.AddDynamic(Component, &UNelaricPawnInitializationTestComponent::RecordInitialization);
	Component->RegisterComponent();
	TestFalse(TEXT("Initialization waits for BeginPlay"), Component->TryInitializePawn());
	TestEqual(TEXT("No early readiness check"), Component->ReadinessChecks, 0);

	Pawn->DispatchBeginPlay();
	TestFalse(TEXT("Missing required context keeps initialization pending"), Component->IsPawnInitialized());
	TestEqual(TEXT("BeginPlay checks readiness"), Component->ReadinessChecks, 1);
	TestEqual(TEXT("Pending initialization does not broadcast"), Component->InitializationEvents, 0);

	Component->bReady = true;
	TestTrue(TEXT("Retry completes initialization"), Component->TryInitializePawn());
	TestTrue(TEXT("Completed state is observable"), Component->IsPawnInitialized());
	TestEqual(TEXT("Successful initialization broadcasts once"), Component->InitializationEvents, 1);
	TestTrue(TEXT("Repeated calls remain successful"), Component->TryInitializePawn());
	TestEqual(TEXT("Repeated calls do not recheck readiness"), Component->ReadinessChecks, 2);
	TestEqual(TEXT("Repeated calls do not broadcast"), Component->InitializationEvents, 1);

	Component->EndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("EndPlay clears initialized state"), Component->IsPawnInitialized());
	TestFalse(TEXT("Ended component cannot initialize again"), Component->TryInitializePawn());
	return true;
}

#endif
