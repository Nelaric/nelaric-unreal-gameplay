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
	TestFalse(TEXT("Ready cannot commit before DataInitialized"), Standalone->CommitReadyWithoutNotification());
	TestTrue(TEXT("Preparation advances to DataInitialized"), Standalone->TryChangeInitState());
	TestFalse(TEXT("TryChange never enters Ready"), Standalone->TryChangeInitState());
	TestFalse(TEXT("Internal preparation gates Ready"), Standalone->CanEnterReady());
	UNelaricInitStateTestPawnComponent* First = NewObject<UNelaricInitStateTestPawnComponent>(Pawn);
	UNelaricInitStateTestPawnComponent* Dependent = NewObject<UNelaricInitStateTestPawnComponent>(Pawn);
	First->bAllowAdvance = true;
	Dependent->bAllowAdvance = true;
	Dependent->bInternalReady = true;
	Subsystem->ConfigureParticipant(Dependent, TEXT("Dependent"), {TEXT("First")});
	First->RegisterComponent();
	Dependent->RegisterComponent();
	TestEqual(TEXT("Own preparation proceeds without a ready dependency"), Dependent->GetInitState(),
	          Nelaric::EInitState::DataInitialized);
	TestFalse(TEXT("Missing dependency is recoverable"), Dependent->HasTerminalInitFailure());
	TestEqual(TEXT("Internal preparation blocks Ready"), First->GetInitState(), Nelaric::EInitState::DataInitialized);
	TestTrue(TEXT("Internal gate is independent of unresolved dependencies"), Dependent->CanEnterReady());
	First->bInternalReady = true;
	First->RequestInitRefresh();
	TestEqual(TEXT("Dependency reaches ready"), First->GetInitState(), Nelaric::EInitState::Ready);
	TestEqual(TEXT("Missing reference remains blocked"), Dependent->GetInitState(),
	          Nelaric::EInitState::DataInitialized);
	TestFalse(TEXT("Missing reference does not fail the attempt"), Dependent->HasTerminalInitFailure());
	Subsystem->ConfigureParticipant(First, TEXT("First"), {});
	TestEqual(TEXT("Configured ID unlocks Ready"), Dependent->GetInitState(), Nelaric::EInitState::Ready);
	TestFalse(TEXT("Ready cannot advance"), First->TryChangeInitState());

	First->MarkTerminalInitFailure();
	TestTrue(TEXT("Failure flag is component-owned"), First->HasTerminalInitFailure());
	TestFalse(TEXT("Dependency failure invalidates a ready dependent"),
	          Dependent->GetInitState() == Nelaric::EInitState::Ready);
	const Nelaric::FInitGeneration OldGeneration = First->GetInitGeneration();
	TestFalse(TEXT("Failed attempt rejects asynchronous result"), First->CanApplyInitResult(World, OldGeneration));
	TestNull(TEXT("Weak completion rejects failed attempt"),
	         UNelaricPawnInitStateComponent::ResolveInitResult(TWeakObjectPtr<UNelaricPawnInitStateComponent>(First),
	                                                           World, OldGeneration));
	First->bAllowAdvance = false;
	First->InvalidateInitGeneration();
	TestFalse(TEXT("Invalidation clears failure"), First->HasTerminalInitFailure());
	TestEqual(TEXT("Invalidation resets stage"), First->GetInitState(), Nelaric::EInitState::Registered);
	TestTrue(TEXT("Invalidation changes generation"), !(OldGeneration == First->GetInitGeneration()));
	TestFalse(TEXT("Old asynchronous result is rejected"), First->CanApplyInitResult(World, OldGeneration));
	TestNull(TEXT("Weak completion rejects old generation"),
	         UNelaricPawnInitStateComponent::ResolveInitResult(TWeakObjectPtr<UNelaricPawnInitStateComponent>(First),
	                                                           World, OldGeneration));
	TestTrue(TEXT("New asynchronous result is accepted"), First->CanApplyInitResult(World, First->GetInitGeneration()));
	TestEqual(TEXT("Weak completion resolves current attempt"),
	          UNelaricPawnInitStateComponent::ResolveInitResult(TWeakObjectPtr<UNelaricPawnInitStateComponent>(First),
	                                                            World, First->GetInitGeneration()),
	          static_cast<UNelaricPawnInitStateComponent*>(First));
	TestNull(TEXT("Weak completion rejects missing component"),
	         UNelaricPawnInitStateComponent::ResolveInitResult(TWeakObjectPtr<UNelaricPawnInitStateComponent>(), World,
	                                                           First->GetInitGeneration()));
	TestEqual(TEXT("Invalidation cancels old work"), First->CancelCount, 1);

	UNelaricInitStateTestDirectComponent* Direct = NewObject<UNelaricInitStateTestDirectComponent>(Pawn);
	Direct->bAllowAdvance = true;
	Direct->bInternalReady = false;
	Direct->RegisterComponent();
	TestTrue(TEXT("UE reflection discovers direct participant"),
	         Direct->GetClass()->ImplementsInterface(UNelaricInitStateParticipantInterface::StaticClass()));
	TestNotNull(TEXT("Direct participant can be cast from UActorComponent"),
	            Cast<INelaricInitStateParticipantInterface>(Direct));
	Subsystem->RegisterParticipant(Direct);
	TestEqual(TEXT("Direct participant stops at internal gate"), Direct->GetInitState(),
	          Nelaric::EInitState::DataInitialized);
	Direct->bInternalReady = true;
	TestTrue(TEXT("Direct internal Ready gate passes"), Direct->CanEnterReady());
	const int32 NotificationsBeforeReady = Direct->NotificationCount;
	TestTrue(TEXT("Direct Ready state writes without callbacks"), Direct->CommitReadyWithoutNotification());
	TestEqual(TEXT("Silent commit does not notify"), Direct->NotificationCount, NotificationsBeforeReady);
	TestFalse(TEXT("Direct Ready cannot commit twice"), Direct->CommitReadyWithoutNotification());
	const Nelaric::FInitStateSnapshot BeforeDirectReady{Direct->GetInitGeneration(),
	                                                    Nelaric::EInitState::DataInitialized, false};
	Direct->NotifyReadyCommitted(BeforeDirectReady);
	TestEqual(TEXT("Direct Ready notifies once"), Direct->NotificationCount, NotificationsBeforeReady + 1);
	Direct->NotifyReadyCommitted(BeforeDirectReady);
	TestEqual(TEXT("Direct Ready notification is idempotent"), Direct->NotificationCount, NotificationsBeforeReady + 1);
	Direct->MarkTerminalInitFailure();
	TestTrue(TEXT("Direct failure is recorded"), Direct->HasTerminalInitFailure());
	const Nelaric::FInitGeneration DirectOldGeneration = Direct->GetInitGeneration();
	Direct->bAllowAdvance = false;
	Direct->InvalidateInitGeneration();
	TestFalse(TEXT("Direct invalidation clears failure"), Direct->HasTerminalInitFailure());
	TestEqual(TEXT("Direct invalidation resets state"), Direct->GetInitState(), Nelaric::EInitState::Registered);
	TestTrue(TEXT("Direct cancellation observes new generation"),
	         Direct->GenerationAtCancel == Direct->GetInitGeneration());
	TestTrue(TEXT("Direct invalidation changes generation"), !(DirectOldGeneration == Direct->GetInitGeneration()));
	Direct->bAllowAdvance = true;
	Direct->MarkTerminalInitFailure();
	Subsystem->RequestParticipantRefresh(Direct);
	TestEqual(TEXT("Terminal failure stops this attempt"), Direct->GetInitState(), Nelaric::EInitState::Registered);
	Direct->InvalidateInitGeneration();
	TestEqual(TEXT("New generation may advance again"), Direct->GetInitState(), Nelaric::EInitState::Ready);
	Subsystem->UnregisterParticipant(Direct);

	UNelaricInitStateTestDirectComponent* GroupA = NewObject<UNelaricInitStateTestDirectComponent>(Pawn);
	UNelaricInitStateTestDirectComponent* GroupB = NewObject<UNelaricInitStateTestDirectComponent>(Pawn);
	GroupA->bAllowAdvance = true;
	GroupB->bAllowAdvance = true;
	GroupA->RegisterComponent();
	GroupB->RegisterComponent();
	Subsystem->RegisterParticipant(GroupA);
	Subsystem->RegisterParticipant(GroupB);
	GroupA->bInternalReady = true;
	GroupB->bInternalReady = true;
	TestTrue(TEXT("All group checks pass before commits"), GroupA->CanEnterReady() && GroupB->CanEnterReady());
	const int32 GroupANotifications = GroupA->NotificationCount;
	const int32 GroupBNotifications = GroupB->NotificationCount;
	const Nelaric::FInitStateSnapshot BeforeGroupA{GroupA->GetInitGeneration(), GroupA->GetInitState(), false};
	const Nelaric::FInitStateSnapshot BeforeGroupB{GroupB->GetInitGeneration(), GroupB->GetInitState(), false};
	TestTrue(TEXT("First group member commits silently"), GroupA->CommitReadyWithoutNotification());
	TestTrue(TEXT("Second group member commits silently"), GroupB->CommitReadyWithoutNotification());
	TestEqual(TEXT("First group member is Ready before notification"), GroupA->GetInitState(),
	          Nelaric::EInitState::Ready);
	TestEqual(TEXT("Second group member is Ready before notification"), GroupB->GetInitState(),
	          Nelaric::EInitState::Ready);
	TestEqual(TEXT("First group member has no early notification"), GroupA->NotificationCount, GroupANotifications);
	TestEqual(TEXT("Second group member has no early notification"), GroupB->NotificationCount, GroupBNotifications);
	GroupA->NotifyReadyCommitted(BeforeGroupA);
	GroupB->NotifyReadyCommitted(BeforeGroupB);
	TestEqual(TEXT("First group member notifies after commits"), GroupA->NotificationCount, GroupANotifications + 1);
	TestEqual(TEXT("Second group member notifies after commits"), GroupB->NotificationCount, GroupBNotifications + 1);
	Subsystem->UnregisterParticipant(GroupA);
	Subsystem->UnregisterParticipant(GroupB);
	return true;
}

#endif
