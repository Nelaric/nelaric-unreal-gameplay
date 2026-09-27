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

	ANelaricPawn* ConfiguredPawn = World->SpawnActor<ANelaricPawn>();
	UNelaricPawnInitializationTestComponent* Manager =
	    NewObject<UNelaricPawnInitializationTestComponent>(ConfiguredPawn);
	UNelaricPawnInitializationConfig* Config = NewObject<UNelaricPawnInitializationConfig>(Manager);
	FNelaricPawnInitializationEntry FirstEntry;
	FirstEntry.ComponentId = TEXT("First");
	FirstEntry.ComponentClass = UNelaricConfiguredInitStateTestComponent::StaticClass();
	FirstEntry.DependencyIds.Add(TEXT("Second"));
	Config->Components.Add(FirstEntry);
	FNelaricPawnInitializationEntry SecondEntry = FirstEntry;
	SecondEntry.ComponentId = TEXT("Second");
	SecondEntry.DependencyIds = {TEXT("First")};
	Config->Components.Add(SecondEntry);
	FNelaricPawnInitializationEntry OptionalEntry;
	OptionalEntry.ComponentId = TEXT("Optional");
	OptionalEntry.ComponentClass = UNelaricInitStateTestPawnComponent::StaticClass();
	OptionalEntry.bRequiredForPawnReady = false;
	Config->Components.Add(OptionalEntry);
	FNelaricPawnInitializationEntry ClientOnlyEntry = FirstEntry;
	ClientOnlyEntry.ComponentId = TEXT("ClientOnly");
	ClientOnlyEntry.DependencyIds.Empty();
	ClientOnlyEntry.bCreateOnAuthority = false;
	Config->Components.Add(ClientOnlyEntry);
	Manager->SetConfig(Config);
	Manager->bReady = true;
	Manager->RegisterComponent();
	ConfiguredPawn->DispatchBeginPlay();
	UActorComponent* FirstCreated = FindObject<UActorComponent>(ConfiguredPawn, TEXT("NelaricInit_First"));
	UActorComponent* SecondCreated = FindObject<UActorComponent>(ConfiguredPawn, TEXT("NelaricInit_Second"));
	TestNotNull(TEXT("Stable ID names first instance"), FirstCreated);
	TestNotNull(TEXT("Stable ID names second instance"), SecondCreated);
	TestTrue(TEXT("First registers after both instances exist"),
	         Cast<UNelaricConfiguredInitStateTestComponent>(FirstCreated) &&
	             Cast<UNelaricConfiguredInitStateTestComponent>(FirstCreated)->bPeerExistsOnRegister);
	TestTrue(TEXT("Second registers after both instances exist"),
	         Cast<UNelaricConfiguredInitStateTestComponent>(SecondCreated) &&
	             Cast<UNelaricConfiguredInitStateTestComponent>(SecondCreated)->bPeerExistsOnRegister);
	TestNotNull(TEXT("Optional component is created"),
	            FindObject<UActorComponent>(ConfiguredPawn, TEXT("NelaricInit_Optional")));
	TestNull(TEXT("Client-only component is absent on authority"),
	         FindObject<UActorComponent>(ConfiguredPawn, TEXT("NelaricInit_ClientOnly")));
	if (!FirstCreated || !SecondCreated)
	{
		return false;
	}
	TestTrue(TEXT("Configured cycle reaches Ready together"), Manager->TryInitializePawn());
	TestTrue(TEXT("Created instances are actor-managed"),
	         ConfiguredPawn->GetInstanceComponents().Contains(FirstCreated) &&
	             ConfiguredPawn->GetInstanceComponents().Contains(SecondCreated));
	TestTrue(TEXT("Repeated initialization keeps the first instance"), Manager->TryInitializePawn());
	TestEqual(TEXT("Stable ID is not duplicated"), FindObject<UActorComponent>(ConfiguredPawn, TEXT("NelaricInit_First")),
	          FirstCreated);
	TestEqual(TEXT("First cycle member is Ready"),
	          Cast<UNelaricConfiguredInitStateTestComponent>(FirstCreated)->GetInitState(), Nelaric::EInitState::Ready);
	TestEqual(TEXT("Second cycle member is Ready"),
	          Cast<UNelaricConfiguredInitStateTestComponent>(SecondCreated)->GetInitState(),
	          Nelaric::EInitState::Ready);
	UNelaricConfiguredInitStateTestComponent* FirstState = Cast<UNelaricConfiguredInitStateTestComponent>(FirstCreated);
	FirstState->bInternalReady = false;
	FirstState->InvalidateInitGeneration();
	TestFalse(TEXT("Cycle partner invalidates with its dependency"),
	          Cast<UNelaricConfiguredInitStateTestComponent>(SecondCreated)->GetInitState() ==
	              Nelaric::EInitState::Ready);
	TestFalse(TEXT("Required invalidation blocks pawn readiness"), Manager->TryInitializePawn());
	TestFalse(TEXT("Required invalidation clears pawn readiness"), Manager->IsPawnInitialized());

	auto RejectConfig = [this, World](const TArray<FNelaricPawnInitializationEntry>& Entries,
	                                  const TCHAR* ExpectedError, bool bAddConflictingInstance = false)
	{
		ANelaricPawn* InvalidPawn = World->SpawnActor<ANelaricPawn>();
		UNelaricPawnInitializationTestComponent* InvalidManager =
		    NewObject<UNelaricPawnInitializationTestComponent>(InvalidPawn);
		UNelaricPawnInitializationConfig* InvalidConfig = NewObject<UNelaricPawnInitializationConfig>(InvalidManager);
		InvalidConfig->Components = Entries;
		InvalidManager->SetConfig(InvalidConfig);
		InvalidManager->bReady = true;
		InvalidManager->RegisterComponent();
		if (bAddConflictingInstance)
		{
			UActorComponent* Existing =
			    NewObject<UNelaricPawnInitializationComponent>(InvalidPawn, TEXT("NelaricInit_Second"));
			InvalidPawn->AddInstanceComponent(Existing);
		}
		AddExpectedError(ExpectedError, EAutomationExpectedErrorFlags::Contains, 1);
		InvalidPawn->DispatchBeginPlay();
		TestFalse(TEXT("Invalid configuration remains pending"), InvalidManager->TryInitializePawn());
		TestFalse(TEXT("Invalid configuration is not Ready"), InvalidManager->IsPawnInitialized());
		TestNull(TEXT("Validation creates no earlier component"),
		         FindObject<UActorComponent>(InvalidPawn, TEXT("NelaricInit_First")));
	};

	FNelaricPawnInitializationEntry PlainFirst = FirstEntry;
	PlainFirst.DependencyIds.Empty();
	FNelaricPawnInitializationEntry PlainSecond = PlainFirst;
	PlainSecond.ComponentId = TEXT("Second");
	RejectConfig({PlainFirst, PlainFirst}, TEXT("duplicate component ID"));
	FNelaricPawnInitializationEntry EmptyId = PlainSecond;
	EmptyId.ComponentId = NAME_None;
	RejectConfig({PlainFirst, EmptyId}, TEXT("empty component ID"));
	FNelaricPawnInitializationEntry MissingClass = PlainSecond;
	MissingClass.ComponentClass = nullptr;
	RejectConfig({PlainFirst, MissingClass}, TEXT("missing component class"));
	FNelaricPawnInitializationEntry WrongClass = PlainSecond;
	WrongClass.ComponentClass = UNelaricPawnInitializationComponent::StaticClass();
	RejectConfig({PlainFirst, WrongClass}, TEXT("does not implement the init-state participant interface"));
	FNelaricPawnInitializationEntry DuplicateDependency = PlainSecond;
	DuplicateDependency.DependencyIds = {TEXT("First"), TEXT("First")};
	RejectConfig({PlainFirst, DuplicateDependency}, TEXT("duplicate dependency declaration"));
	FNelaricPawnInitializationEntry MissingDependency = PlainSecond;
	MissingDependency.DependencyIds = {TEXT("Absent")};
	RejectConfig({PlainFirst, MissingDependency}, TEXT("dependency ID does not exist"));
	FNelaricPawnInitializationEntry ServerOnly = PlainSecond;
	ServerOnly.bCreateOnClient = false;
	FNelaricPawnInitializationEntry ClientDependent = PlainFirst;
	ClientDependent.DependencyIds = {TEXT("Second")};
	RejectConfig({ClientDependent, ServerOnly}, TEXT("dependency is not created on clients"));
	FNelaricPawnInitializationEntry ClientOnly = PlainSecond;
	ClientOnly.bCreateOnAuthority = false;
	FNelaricPawnInitializationEntry ServerDependent = PlainFirst;
	ServerDependent.DependencyIds = {TEXT("Second")};
	RejectConfig({ServerDependent, ClientOnly}, TEXT("dependency is not created on authority"));
	RejectConfig({PlainFirst, PlainSecond}, TEXT("instance 'NelaricInit_Second' has a conflicting class"), true);
	return true;
}

#endif
