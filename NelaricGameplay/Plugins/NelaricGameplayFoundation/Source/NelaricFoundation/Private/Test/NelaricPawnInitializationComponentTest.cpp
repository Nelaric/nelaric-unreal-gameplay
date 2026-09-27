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
	Component->OnPawnInitializationRevoked.AddDynamic(Component,
	                                                  &UNelaricPawnInitializationTestComponent::RecordRevocation);
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
	TestEqual(TEXT("Repeated calls recheck current readiness"), Component->ReadinessChecks, 5);
	TestEqual(TEXT("Repeated calls do not broadcast"), Component->InitializationEvents, 1);
	Component->bReady = false;
	TestFalse(TEXT("Changed pawn condition clears current conclusion"), Component->IsPawnInitialized());
	TestFalse(TEXT("Retry revokes pawn Ready"), Component->TryInitializePawn());
	TestEqual(TEXT("Revocation is announced once"), Component->RevocationEvents, 1);
	Component->bReady = true;
	TestTrue(TEXT("Pawn condition can recover"), Component->TryInitializePawn());
	TestEqual(TEXT("Recovery announces Ready again"), Component->InitializationEvents, 2);

	Component->EndPlay(EEndPlayReason::Destroyed);
	TestFalse(TEXT("EndPlay clears initialized state"), Component->IsPawnInitialized());
	TestFalse(TEXT("Ended component cannot initialize again"), Component->TryInitializePawn());

	ANelaricPawn* ConfiguredPawn = World->SpawnActor<ANelaricPawn>();
	UNelaricPawnInitializationTestComponent* Manager =
	    NewObject<UNelaricPawnInitializationTestComponent>(ConfiguredPawn);
	UNelaricPawnInitializationConfig* Config = NewObject<UNelaricPawnInitializationConfig>(Manager);
	FNelaricPawnInitializationEntry FirstEntry;
	FirstEntry.ComponentId = TEXT("First");
	FirstEntry.ComponentClass = UNelaricReplicatingConfiguredInitStateTestComponent::StaticClass();
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
	Manager->OnPawnInitialized.AddDynamic(Manager, &UNelaricPawnInitializationTestComponent::RecordInitialization);
	Manager->OnPawnInitializationRevoked.AddDynamic(Manager,
	                                                &UNelaricPawnInitializationTestComponent::RecordRevocation);
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
	UNelaricInitStateTestPawnComponent* Unlisted = NewObject<UNelaricInitStateTestPawnComponent>(ConfiguredPawn);
	Unlisted->RegisterComponent();
	TestTrue(TEXT("Unlisted participant does not block pawn Ready"), Manager->IsPawnInitialized());
	UNelaricInitStateTestPawnComponent* OptionalState = Cast<UNelaricInitStateTestPawnComponent>(
	    FindObject<UActorComponent>(ConfiguredPawn, TEXT("NelaricInit_Optional")));
	TestTrue(TEXT("Optional configured member remains unready"),
	         OptionalState && OptionalState->GetInitState() != Nelaric::EInitState::Ready);
	TestTrue(TEXT("Optional member does not block pawn Ready"), Manager->IsPawnInitialized());
	TestTrue(TEXT("Created instances are actor-managed"),
	         ConfiguredPawn->GetInstanceComponents().Contains(FirstCreated) &&
	             ConfiguredPawn->GetInstanceComponents().Contains(SecondCreated));
	TestTrue(TEXT("Test class defaults to replicated"),
	         UNelaricReplicatingConfiguredInitStateTestComponent::StaticClass()
	             ->GetDefaultObject<UActorComponent>()
	             ->GetIsReplicated());
	TestFalse(TEXT("Configured authority instance does not replicate"), FirstCreated->GetIsReplicated());
	TestFalse(TEXT("Second configured authority instance does not replicate"), SecondCreated->GetIsReplicated());
	ANelaricClientRoleInitializationTestPawn* ClientPawn =
	    World->SpawnActor<ANelaricClientRoleInitializationTestPawn>();
	ClientPawn->SimulateClientRole();
	UNelaricPawnInitializationTestComponent* ClientManager =
	    NewObject<UNelaricPawnInitializationTestComponent>(ClientPawn);
	ClientManager->SetConfig(Config);
	ClientManager->bReady = true;
	ClientManager->RegisterComponent();
	ClientPawn->DispatchBeginPlay();
	UActorComponent* ClientFirst = FindObject<UActorComponent>(ClientPawn, TEXT("NelaricInit_First"));
	UActorComponent* ClientSecond = FindObject<UActorComponent>(ClientPawn, TEXT("NelaricInit_Second"));
	TestNotNull(TEXT("Client creates its own first instance"), ClientFirst);
	TestNotNull(TEXT("Client creates its own second instance"), ClientSecond);
	TestTrue(TEXT("Same ID has distinct local instances"), ClientFirst && ClientFirst != FirstCreated);
	TestTrue(TEXT("Client instance is owned by client pawn"), ClientFirst && ClientFirst->GetOwner() == ClientPawn);
	TestTrue(TEXT("Client selected client-only entry"),
	         FindObject<UActorComponent>(ClientPawn, TEXT("NelaricInit_ClientOnly")) != nullptr);
	TestTrue(TEXT("Client initialization reaches Ready"), ClientManager->IsPawnInitialized());
	TestTrue(TEXT("Client configured instance does not replicate"), ClientFirst && !ClientFirst->GetIsReplicated());
	TestTrue(TEXT("Client second instance does not replicate"), ClientSecond && !ClientSecond->GetIsReplicated());
	if (!ClientFirst || !ClientSecond)
	{
		return false;
	}
	const Nelaric::FInitGeneration ClientGeneration =
	    Cast<UNelaricConfiguredInitStateTestComponent>(ClientFirst)->GetInitGeneration();
	TestTrue(TEXT("Repeated initialization keeps the first instance"), Manager->TryInitializePawn());
	TestEqual(TEXT("Stable ID is not duplicated"),
	          FindObject<UActorComponent>(ConfiguredPawn, TEXT("NelaricInit_First")), FirstCreated);
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
	TestEqual(TEXT("Required invalidation announces revocation"), Manager->RevocationEvents, 1);
	FirstState->bInternalReady = true;
	FirstState->RequestInitRefresh();
	TestTrue(TEXT("Required cycle can recover"), Manager->IsPawnInitialized());
	TestEqual(TEXT("Recovery announces another Ready transition"), Manager->InitializationEvents, 2);
	FirstState->MarkTerminalInitFailure();
	TestFalse(TEXT("Required terminal failure revokes pawn Ready"), Manager->IsPawnInitialized());
	TestEqual(TEXT("Terminal failure announces revocation"), Manager->RevocationEvents, 2);
	FirstState->InvalidateInitGeneration();
	FirstState->RequestInitRefresh();
	TestTrue(TEXT("Required failure can recover in a new generation"), Manager->IsPawnInitialized());
	TestEqual(TEXT("Failure recovery announces Ready"), Manager->InitializationEvents, 3);

	UNelaricPawnInitializationConfig* Replacement = NewObject<UNelaricPawnInitializationConfig>(Manager);
	FNelaricPawnInitializationEntry ReplacementEntry = FirstEntry;
	ReplacementEntry.ComponentId = TEXT("Replacement");
	ReplacementEntry.DependencyIds.Empty();
	Replacement->Components = {ReplacementEntry};
	Manager->SetConfig(Replacement);
	TestTrue(TEXT("New configuration creates a complete round immediately"), Manager->IsPawnInitialized());
	TestFalse(TEXT("Old instance leaves actor ownership"),
	          ConfiguredPawn->GetInstanceComponents().Contains(FirstCreated));
	TestNotNull(TEXT("Replacement instance uses configured ID"),
	            FindObject<UActorComponent>(ConfiguredPawn, TEXT("NelaricInit_Replacement")));
	TestEqual(TEXT("Configuration replacement announces revocation"), Manager->RevocationEvents, 3);
	TestEqual(TEXT("Configuration replacement announces Ready"), Manager->InitializationEvents, 4);
	UNelaricPawnInitializationConfig* InvalidReplacement = NewObject<UNelaricPawnInitializationConfig>(Manager);
	FNelaricPawnInitializationEntry InvalidReplacementEntry = ReplacementEntry;
	InvalidReplacementEntry.ComponentId = NAME_None;
	InvalidReplacement->Components = {ReplacementEntry, InvalidReplacementEntry};
	AddExpectedError(TEXT("empty component ID"), EAutomationExpectedErrorFlags::Contains, 1);
	Manager->SetConfig(InvalidReplacement);
	TestFalse(TEXT("Invalid replacement cannot enter Ready"), Manager->TryInitializePawn());
	TestNull(TEXT("Invalid replacement creates no partial round"),
	         FindObject<UActorComponent>(ConfiguredPawn, TEXT("NelaricInit_Replacement")));
	TestEqual(TEXT("Invalid replacement revokes old Ready"), Manager->RevocationEvents, 4);
	Manager->SetConfig(Replacement);
	TestTrue(TEXT("Valid replacement recovers after invalid round"), Manager->TryInitializePawn());
	TestEqual(TEXT("Recovered replacement announces Ready"), Manager->InitializationEvents, 5);
	TestTrue(TEXT("Authority invalidation leaves client conclusion local"), ClientManager->IsPawnInitialized());
	TestTrue(TEXT("Authority invalidation leaves client generation local"),
	         ClientGeneration == Cast<UNelaricConfiguredInitStateTestComponent>(ClientFirst)->GetInitGeneration());
	ANelaricPawn* OptionalOnlyPawn = World->SpawnActor<ANelaricPawn>();
	UNelaricPawnInitializationTestComponent* OptionalOnlyManager =
	    NewObject<UNelaricPawnInitializationTestComponent>(OptionalOnlyPawn);
	UNelaricPawnInitializationConfig* OptionalOnlyConfig =
	    NewObject<UNelaricPawnInitializationConfig>(OptionalOnlyManager);
	OptionalOnlyConfig->Components = {OptionalEntry};
	OptionalOnlyManager->SetConfig(OptionalOnlyConfig);
	OptionalOnlyManager->bReady = true;
	OptionalOnlyManager->RegisterComponent();
	OptionalOnlyPawn->DispatchBeginPlay();
	TestNotNull(TEXT("Optional-only configuration still creates its instance"),
	            FindObject<UActorComponent>(OptionalOnlyPawn, TEXT("NelaricInit_Optional")));
	TestTrue(TEXT("No required entries allow pawn Ready"), OptionalOnlyManager->IsPawnInitialized());

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
	ANelaricPawn* CollisionPawn = World->SpawnActor<ANelaricPawn>();
	UNelaricPawnInitializationTestComponent* CollisionManager =
	    NewObject<UNelaricPawnInitializationTestComponent>(CollisionPawn);
	UNelaricPawnInitializationConfig* CollisionConfig = NewObject<UNelaricPawnInitializationConfig>(CollisionManager);
	CollisionConfig->Components = {PlainFirst};
	CollisionManager->SetConfig(CollisionConfig);
	CollisionManager->bReady = true;
	CollisionManager->RegisterComponent();
	UActorComponent* ExternalInstance =
	    NewObject<UNelaricReplicatingConfiguredInitStateTestComponent>(CollisionPawn, TEXT("NelaricInit_First"));
	CollisionPawn->AddInstanceComponent(ExternalInstance);
	AddExpectedError(TEXT("already exists outside the initialization component"),
	                 EAutomationExpectedErrorFlags::Contains, 1);
	CollisionPawn->DispatchBeginPlay();
	TestFalse(TEXT("External same-ID instance is not adopted"), CollisionManager->IsPawnInitialized());
	return true;
}

#endif
