// Copyright (c) 2026 Nelaric

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Pawn/NelaricPawnComponent.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AssertionMacros.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Pawn/NelaricCharacter.h"
#include "Pawn/NelaricPawn.h"
#include "Player/NelaricPlayerController.h"
#include "Templates/Function.h"
#include "Test/NelaricPawnComponentTestTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNelaricPawnContextLifecycleTest, "Nelaric.Foundation.Pawn.ContextLifecycle",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNelaricPawnContextLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	ANelaricPawn* Pawn = World->SpawnActor<ANelaricPawn>();
	UNelaricPawnComponentTestComponent* Component = NewObject<UNelaricPawnComponentTestComponent>(Pawn);
	Component->RegisterComponent();
	TestTrue(TEXT("Direct pawn owner is available"), Component->GetPawn() == Pawn);
	TestTrue(TEXT("Typed pawn query succeeds"), Component->GetPawn<APawn>() == Pawn);
	TestTrue(TEXT("Project pawn query succeeds"), Component->GetPawn<ANelaricPawn>() == Pawn);
	TestNull(TEXT("Wrong pawn subtype is rejected"), Component->GetPawn<ACharacter>());
	TestNull(TEXT("Unpossessed pawn has no controller"), Component->GetController());
	TestNull(TEXT("Unpossessed pawn has no player state"), Component->GetPlayerState());
	TestFalse(TEXT("Unpossessed pawn is not locally controlled"), Component->IsLocallyControlled());
	TestFalse(TEXT("Tick is disabled by default"), Component->PrimaryComponentTick.bCanEverTick);
	TestFalse(TEXT("Replication is disabled by default"), Component->GetIsReplicated());

	ANelaricPlayerController* FirstController = World->SpawnActor<ANelaricPlayerController>();
	FirstController->Possess(Pawn);
	TestTrue(TEXT("Possession is visible without refreshing"), Component->GetController() == FirstController);
	TestTrue(TEXT("Typed controller query succeeds"),
	         Component->GetController<ANelaricPlayerController>() == FirstController);
	TestTrue(TEXT("Player controller query succeeds"), Component->GetPlayerController() == FirstController);
	TestTrue(TEXT("Typed player controller query succeeds"),
	         Component->GetPlayerController<ANelaricPlayerController>() == FirstController);
	TestEqual(TEXT("Local control follows the pawn"), Component->IsLocallyControlled(), Pawn->IsLocallyControlled());

	APlayerController* SecondController = World->SpawnActor<APlayerController>();
	SecondController->Possess(Pawn);
	TestTrue(TEXT("Replacement controller is returned"), Component->GetController() == SecondController);
	TestNull(TEXT("Wrong player controller subtype is rejected"),
	         Component->GetPlayerController<ANelaricPlayerController>());
	SecondController->UnPossess();
	TestNull(TEXT("Unpossession clears the query"), Component->GetController());
	TestNull(TEXT("Unpossession clears the player controller query"), Component->GetPlayerController());

	ANelaricPawnComponentTestController* NonPlayerController = World->SpawnActor<ANelaricPawnComponentTestController>();
	NonPlayerController->Possess(Pawn);
	TestTrue(TEXT("Non-player possession is supported"), Component->GetController() == NonPlayerController);
	TestNull(TEXT("Non-player controller is not a player controller"), Component->GetPlayerController());
	TestNull(TEXT("Wrong controller subtype is rejected"), Component->GetController<APlayerController>());
	TestEqual(TEXT("Non-player local control follows the pawn"), Component->IsLocallyControlled(),
	          Pawn->IsLocallyControlled());
	NonPlayerController->UnPossess();

	ANelaricPawnComponentTestPlayerState* FirstState = World->SpawnActor<ANelaricPawnComponentTestPlayerState>();
	Pawn->SetPlayerState(FirstState);
	TestNull(TEXT("State query scenario has no controller"), Component->GetController());
	TestTrue(TEXT("Player state is accessible without a controller"), Component->GetPlayerState() == FirstState);
	TestTrue(TEXT("Typed player state query succeeds"),
	         Component->GetPlayerState<ANelaricPawnComponentTestPlayerState>() == FirstState);
	TestEqual(TEXT("Player control follows the pawn without a controller"), Component->IsPlayerControlled(),
	          Pawn->IsPlayerControlled());

	APlayerState* SecondState = World->SpawnActor<APlayerState>();
	Pawn->SetPlayerState(SecondState);
	TestTrue(TEXT("Replacement player state is returned"), Component->GetPlayerState() == SecondState);
	TestNull(TEXT("Wrong player state subtype is rejected"),
	         Component->GetPlayerState<ANelaricPawnComponentTestPlayerState>());
	SecondState->Destroy();
	TestNull(TEXT("Destroyed player state is filtered"), Component->GetPlayerState());
	Pawn->SetPlayerState(nullptr);
	TestNull(TEXT("Removed player state is absent"), Component->GetPlayerState());

	ANelaricCharacter* Character = World->SpawnActor<ANelaricCharacter>();
	UNelaricPawnComponentTestComponent* CharacterComponent = NewObject<UNelaricPawnComponentTestComponent>(Character);
	CharacterComponent->RegisterComponent();
	TestTrue(TEXT("Character owner is supported"), CharacterComponent->GetPawn<ACharacter>() == Character);
	TestTrue(TEXT("Project character query succeeds"), CharacterComponent->GetPawn<ANelaricCharacter>() == Character);
	TestTrue(TEXT("Character component remains with its own pawn"), Component->GetPawn() == Pawn);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNelaricPawnInvalidContextTest, "Nelaric.Foundation.Pawn.InvalidContext",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FNelaricPawnInvalidContextTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	};

	const UNelaricPawnComponentTestComponent* Template = GetDefault<UNelaricPawnComponentTestComponent>();
	TestNull(TEXT("Template has no pawn context"), Template->GetPawn());

	APawn* Pawn = World->SpawnActor<APawn>();
	AActor* Actor = World->SpawnActor<AActor>();
	Actor->SetOwner(Pawn);
	UNelaricPawnComponentTestComponent* Component = NewObject<UNelaricPawnComponentTestComponent>(Actor);
	{
		// Handle this expected ensure without swallowing unrelated diagnostics.
		// ensureMsgf only reports once, so repeated test runs need no log count.
		auto PreviousEnsureHandler = SetEnsureHandler(
		    [](const FEnsureHandlerArgs& Args)
		    { return FString(Args.Message).StartsWith(TEXT("NelaricPawnComponent requires a Pawn owner:")); });
		ON_SCOPE_EXIT
		{
			SetEnsureHandler(MoveTemp(PreviousEnsureHandler));
		};
		Component->RegisterComponent();
	}
	TestNull(TEXT("Actor ownership chain is not traversed"), Component->GetPawn());
	TestNull(TEXT("Invalid owner has no controller"), Component->GetController());
	TestNull(TEXT("Invalid owner has no player controller"), Component->GetPlayerController());
	TestNull(TEXT("Invalid owner has no player state"), Component->GetPlayerState());
	TestFalse(TEXT("Invalid owner is not locally controlled"), Component->IsLocallyControlled());
	TestFalse(TEXT("Invalid owner is not player controlled"), Component->IsPlayerControlled());
	return true;
}

#endif
