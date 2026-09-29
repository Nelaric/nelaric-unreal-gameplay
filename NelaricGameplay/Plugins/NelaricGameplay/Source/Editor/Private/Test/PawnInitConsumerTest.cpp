// Copyright (c) 2026 Nelaric

#if WITH_DEV_AUTOMATION_TESTS

#include "Test/PawnInitConsumerTestTypes.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Pawn/InitStateWorldSubsystem.h"
#include "Pawn/NelaricCharacter.h"
#include "Pawn/PawnInitializationConfig.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPawnPublicConsumerTest, "Nelaric.GameplayRuntime.Pawn.PublicConsumer",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPawnPublicConsumerTest::RunTest(const FString& Parameters)
{
	// This translation unit belongs to Editor and includes only runtime Public headers.
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};
	AInitConsumerPawn* Pawn = World->SpawnActor<AInitConsumerPawn>();
	UInitConsumerManager* Manager = Cast<UInitConsumerManager>(Pawn->GetPawnInitializationComponent());
	if (!TestNotNull(TEXT("External module can replace the default manager"), Manager))
	{
		return false;
	}
	UPawnInitializationConfig* Config = NewObject<UPawnInitializationConfig>(Pawn);
	FPawnInitializationEntry Entry;
	Entry.ComponentId = TEXT("Consumer");
	Entry.ComponentClass = UInitConsumerComponent::StaticClass();
	Config->Components.Add(Entry);
	Manager->SetInitializationConfig(Config);
	TStrongObjectPtr<UInitConsumerObserver> Observer(NewObject<UInitConsumerObserver>());
	FPawnInitializationCallback Ready;
	Ready.BindDynamic(Observer.Get(), &UInitConsumerObserver::OnReady);
	FPawnInitializationCallback Revoked;
	Revoked.BindDynamic(Observer.Get(), &UInitConsumerObserver::OnRevoked);
	Manager->RegisterPawnInitializationRevoked(Revoked);
	Manager->RegisterAndCallPawnInitialized(Ready);
	Pawn->DispatchBeginPlay();
	UInitConsumerComponent* Component = Pawn->FindComponentByClass<UInitConsumerComponent>();
	if (!TestNotNull(TEXT("External component is created from public config"), Component))
	{
		return false;
	}
	TestFalse(TEXT("External data gate keeps initialization pending"), Manager->IsPawnInitialized());
	Component->bDataAvailable = true;
	Component->RequestInitRefresh();
	TestEqual(TEXT("External component calls exported base preparation"), Component->ReadyCalls, 1);
	TestFalse(TEXT("Pawn gate remains independent from component Ready"), Manager->IsPawnInitialized());
	Manager->bContextAvailable = true;
	TestTrue(TEXT("External module can retry the public pawn gate"), Manager->TryInitializePawn());
	TestEqual(TEXT("External delegate receives Ready"), Observer->ReadyCalls, 1);
	const Nelaric::FInitGeneration Old = Component->GetInitGeneration();
	Component->InvalidateInitContext();
	TestFalse(TEXT("External component can invalidate context"), Old == Component->GetInitGeneration());
	Manager->InvalidatePawnContext();
	TestTrue(TEXT("External pawn context reset invokes derived cleanup"), Component->CleanupCalls >= 2);
	TestTrue(TEXT("External module can query required participants"),
	         World->GetSubsystem<UInitStateWorldSubsystem>()->AreRequiredParticipantsReady(Pawn));
	TestTrue(TEXT("External observer receives revocation"), Observer->RevokedCalls >= 2);
	Manager->UnregisterPawnInitializationCallback(Ready);
	Manager->UnregisterPawnInitializationCallback(Revoked);
	const int32 ReadyBeforeRemoval = Observer->ReadyCalls;
	Manager->InvalidatePawnContext();
	TestEqual(TEXT("Public unsubscribe removes the external listener"), Observer->ReadyCalls, ReadyBeforeRemoval);
	ANelaricCharacter* Character = World->SpawnActor<ANelaricCharacter>();
	TestNotNull(TEXT("Character accessor links across modules"), Character->GetPawnInitializationComponent());
	return true;
}

#endif
