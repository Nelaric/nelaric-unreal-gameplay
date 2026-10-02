// Copyright (c) 2026 Nelaric Contributors

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Test/PawnInitRegressionTestTypes.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Pawn/NelaricPawn.h"
#include "Pawn/PawnInitializationConfig.h"
#include "UObject/StrongObjectPtr.h"

namespace Nelaric::Tests
{
FPawnInitializationEntry Entry(FName Id, TArray<FName> Dependencies = {}, bool bRequired = true)
{
	FPawnInitializationEntry Result;
	Result.ComponentId = Id;
	Result.ComponentClass = UInitRegressionComponent::StaticClass();
	Result.DependencyIds = MoveTemp(Dependencies);
	Result.bRequiredForPawnReady = bRequired;
	return Result;
}

class FInitFixture final
{
public:
	explicit FInitFixture(TArray<FPawnInitializationEntry> Entries)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
		Pawn = World->SpawnActor<ANelaricPawn>();
		Manager = Pawn->GetPawnInitializationComponent();
		Config = NewObject<UPawnInitializationConfig>(Pawn);
		Config->Components = MoveTemp(Entries);
		Manager->SetInitializationConfig(Config);
		Pawn->DispatchBeginPlay();
		Observer.Reset(NewObject<UInitRegressionObserver>());
		Manager->RegisterPawnInitializationRevoked(RevokedCallback(Observer.Get()));
		Manager->RegisterAndCallPawnInitialized(ReadyCallback(Observer.Get()));
	}

	~FInitFixture()
	{
		World->DestroyWorld(false);
		GEngine->DestroyWorldContext(World);
	}

	UInitRegressionComponent* Node(FName Id) const
	{
		return FindObject<UInitRegressionComponent>(Pawn, *(TEXT("Init_") + Id.ToString()));
	}

	void Complete()
	{
		TArray<UInitRegressionComponent*> Nodes;
		Pawn->GetComponents(Nodes);
		for (UInitRegressionComponent* Component : Nodes)
		{
			Component->bAllowReady = true;
		}
		for (UInitRegressionComponent* Component : Nodes)
		{
			if (IsValid(Component) && Component->IsRegistered())
			{
				Component->RequestInitRefresh();
			}
		}
	}

	static FPawnInitializationCallback ReadyCallback(UInitRegressionObserver* Listener)
	{
		FPawnInitializationCallback Callback;
		Callback.BindDynamic(Listener, &UInitRegressionObserver::OnReady);
		return Callback;
	}

	static FPawnInitializationCallback RevokedCallback(UInitRegressionObserver* Listener)
	{
		FPawnInitializationCallback Callback;
		Callback.BindDynamic(Listener, &UInitRegressionObserver::OnRevoked);
		return Callback;
	}

	UWorld* World = nullptr;
	ANelaricPawn* Pawn = nullptr;
	UPawnInitializationComponent* Manager = nullptr;
	UPawnInitializationConfig* Config = nullptr;
	TStrongObjectPtr<UInitRegressionObserver> Observer;
};
} // namespace Nelaric::Tests

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPawnReadyDependencyRecheckTest, "Nelaric.GameplayRuntime.Pawn.ReadyDependencyRecheck",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPawnReadyDependencyRecheckTest::RunTest(const FString& Parameters)
{
	using namespace Nelaric::Tests;
	for (int32 Mutation = 0; Mutation < 3; ++Mutation)
	{
		FInitFixture Fixture({Entry(TEXT("B")), Entry(TEXT("A"), {TEXT("B")})});
		UInitRegressionComponent* A = Fixture.Node(TEXT("A"));
		UInitRegressionComponent* B = Fixture.Node(TEXT("B"));
		if (!TestNotNull(TEXT("Dependent created"), A) || !TestNotNull(TEXT("Dependency created"), B))
		{
			return false;
		}
		B->bAllowReady = true;
		B->RequestInitRefresh();
		TestEqual(TEXT("Dependency initially Ready"), B->GetInitState(), Nelaric::EInitState::Ready);
		const Nelaric::FInitGeneration OldDependency = B->GetInitGeneration();
		A->PrepareAction = [B, Mutation]
		{
			B->bAllowReady = false;
			if (Mutation == 0)
			{
				B->InvalidateInitContext();
			}
			else if (Mutation == 1)
			{
				B->UnregisterComponent();
			}
			else
			{
				B->MarkTerminalInitFailure();
			}
		};
		A->bAllowReady = true;
		if (Mutation == 2)
		{
			AddExpectedErrorPlain(
			    FString::Printf(TEXT("Terminal initialization failure: component=%s generation=%llu state=%d."),
			                    *B->GetName(), static_cast<unsigned long long>(B->GetInitGeneration().Value),
			                    static_cast<int32>(B->GetInitState())),
			    EAutomationExpectedErrorFlags::Exact, 1);
		}
		A->RequestInitRefresh();
		TestEqual(TEXT("Invalidated dependency prevents Ready callback"), A->ReadyCalls, 0);
		TestFalse(TEXT("Pawn never becomes Ready against an invalid dependency"), Fixture.Manager->IsPawnInitialized());
		TestEqual(TEXT("No premature pawn Ready notification"), Fixture.Observer->ReadyCalls, 0);
		if (Mutation == 2)
		{
			B->RequestInitRefresh();
			TestTrue(TEXT("Ordinary refresh preserves terminal failure"), B->HasTerminalInitFailure());
			B->InvalidateInitContext();
		}
		else
		{
			TestFalse(TEXT("Old dependency generation was invalidated"), B->GetInitGeneration() == OldDependency);
		}
		B->bAllowReady = true;
		if (!B->IsRegistered())
		{
			B->RegisterComponent();
		}
		B->RequestInitRefresh();
		TestTrue(TEXT("Dependency recovery completes the pawn"), Fixture.Manager->IsPawnInitialized());
		TestEqual(TEXT("Recovered dependent starts once"), A->ReadyCalls, 1);
		TestEqual(TEXT("Only recovered pawn Ready is published"), Fixture.Observer->ReadyCalls, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPawnContextInvalidationTest, "Nelaric.GameplayRuntime.Pawn.ContextInvalidation",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPawnContextInvalidationTest::RunTest(const FString& Parameters)
{
	using namespace Nelaric::Tests;
	FInitFixture Fixture({Entry(TEXT("B"), {}, false), Entry(TEXT("A"), {TEXT("B")}), Entry(TEXT("C"), {TEXT("A")}),
	                      Entry(TEXT("Independent"))});
	Fixture.Complete();
	UInitRegressionComponent* A = Fixture.Node(TEXT("A"));
	UInitRegressionComponent* B = Fixture.Node(TEXT("B"));
	UInitRegressionComponent* C = Fixture.Node(TEXT("C"));
	UInitRegressionComponent* Independent = Fixture.Node(TEXT("Independent"));
	TestTrue(TEXT("All required components initially Ready"), Fixture.Manager->IsPawnInitialized());
	const Nelaric::FInitGeneration OldA = A->GetInitGeneration();
	const Nelaric::FInitGeneration OldC = C->GetInitGeneration();
	const Nelaric::FInitGeneration Unrelated = Independent->GetInitGeneration();
	bool bCleanupObservedStoppedPawn = true;
	A->CleanupAction = [&Fixture, &bCleanupObservedStoppedPawn]
	{ bCleanupObservedStoppedPawn &= !Fixture.Observer->bGameplayActive && !Fixture.Manager->IsPawnInitialized(); };
	B->bApplicable = false;
	B->RequestInitRefresh();
	TestFalse(TEXT("Optional dependency loss revokes required pawn Ready"), Fixture.Manager->IsPawnInitialized());
	TestFalse(TEXT("Dependent generation reset"), A->GetInitGeneration() == OldA);
	TestFalse(TEXT("Transitive dependent generation reset"), C->GetInitGeneration() == OldC);
	TestTrue(TEXT("Unrelated component keeps its generation"), Independent->GetInitGeneration() == Unrelated);
	TestTrue(TEXT("Pawn gameplay stops before component cleanup"), bCleanupObservedStoppedPawn);
	TestEqual(TEXT("Single revocation for dependency loss"), Fixture.Observer->RevokedCalls, 1);
	TestFalse(TEXT("Old asynchronous results rejected"), A->CanApplyInitResult(Fixture.World, OldA));
	TestNull(TEXT("Weak asynchronous resolver rejects old generation"),
	         UPawnInitStateComponent::ResolveInitResult(A, Fixture.World, OldA));
	B->bApplicable = true;
	B->RequestInitRefresh();
	TestTrue(TEXT("Restored applicability recovers the pawn"), Fixture.Manager->IsPawnInitialized());
	TestEqual(TEXT("Recovered Ready published once"), Fixture.Observer->ReadyCalls, 2);
	const Nelaric::FInitGeneration BeforeComponentReset = C->GetInitGeneration();
	C->InvalidateInitContext();
	TestFalse(TEXT("Ready context invalidation starts a new generation"),
	          C->GetInitGeneration() == BeforeComponentReset);
	TestTrue(TEXT("New asynchronous results accepted"), C->CanApplyInitResult(Fixture.World, C->GetInitGeneration()));
	TestTrue(TEXT("Weak resolver accepts current generation"),
	         UPawnInitStateComponent::ResolveInitResult(C, Fixture.World, C->GetInitGeneration()) == C);
	const Nelaric::FInitGeneration BeforePawnReset = Independent->GetInitGeneration();
	Fixture.Manager->InvalidatePawnContext();
	TestFalse(TEXT("Pawn reset also invalidates unrelated pawn participants"),
	          Independent->GetInitGeneration() == BeforePawnReset);
	TestTrue(TEXT("Explicit pawn reset recovers without polling"), Fixture.Manager->IsPawnInitialized());
	// Do not retain fixture captures in callbacks invoked by world teardown.
	A->CleanupAction = nullptr;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPawnReadyReentrancyTest, "Nelaric.GameplayRuntime.Pawn.ReadyReentrancy",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPawnReadyReentrancyTest::RunTest(const FString& Parameters)
{
	using namespace Nelaric::Tests;
	for (int32 Mutation = 0; Mutation < 4; ++Mutation)
	{
		FInitFixture Fixture({Entry(TEXT("A"), {TEXT("B")}), Entry(TEXT("B"), {TEXT("A")})});
		UInitRegressionComponent* A = Fixture.Node(TEXT("A"));
		UInitRegressionComponent* B = Fixture.Node(TEXT("B"));
		const Nelaric::FInitGeneration OldA = A->GetInitGeneration();
		const Nelaric::FInitGeneration OldB = B->GetInitGeneration();
		bool bActionExecuted = false;
		bool bWholeGroupReadyAtCallback = false;
		TWeakObjectPtr<UInitRegressionComponent> Unnotified;
		Nelaric::FInitGeneration UnnotifiedOldGeneration;
		TFunction<void(UInitRegressionComponent*)> Mutate = [&](UInitRegressionComponent* Current)
		{
			if (bActionExecuted)
			{
				return;
			}
			bActionExecuted = true;
			UInitRegressionComponent* Other = Current == A ? B : A;
			Unnotified = Other;
			UnnotifiedOldGeneration = Other->GetInitGeneration();
			bWholeGroupReadyAtCallback =
			    A->GetInitState() == Nelaric::EInitState::Ready && B->GetInitState() == Nelaric::EInitState::Ready;
			if (Mutation == 0)
			{
				Other->DestroyComponent();
			}
			else if (Mutation == 1)
			{
				UPawnInitializationConfig* Replacement = NewObject<UPawnInitializationConfig>(Fixture.Pawn);
				Replacement->Components = {Entry(TEXT("Replacement"))};
				Fixture.Manager->SetInitializationConfig(Replacement);
			}
			else if (Mutation == 2)
			{
				Fixture.Manager->InvalidatePawnContext();
			}
			else
			{
				Fixture.Pawn->Destroy();
			}
		};
		A->ReadyAction = [&Mutate, A] { Mutate(A); };
		B->ReadyAction = [&Mutate, B] { Mutate(B); };
		Fixture.Complete();
		TestTrue(TEXT("Mutation ran inside the first Ready callback"), bActionExecuted);
		TestTrue(TEXT("All cyclic peers are Ready before notification"), bWholeGroupReadyAtCallback);
		if (UInitRegressionComponent* Other = Unnotified.Get())
		{
			TestFalse(TEXT("Old round never notifies the remaining peer"),
			          Other->ReadyGenerations.Contains(UnnotifiedOldGeneration));
		}
		if (Mutation == 2)
		{
			TestFalse(TEXT("Context restart replaces A's round"), A->GetInitGeneration() == OldA);
			TestFalse(TEXT("Context restart replaces B's round"), B->GetInitGeneration() == OldB);
			TestTrue(TEXT("New cyclic round recovers"), Fixture.Manager->IsPawnInitialized());
			TestEqual(TEXT("Only the final round publishes pawn Ready"), Fixture.Observer->ReadyCalls, 1);
		}
		else
		{
			TestFalse(TEXT("Invalidated old graph cannot publish pawn Ready"), Fixture.Manager->IsPawnInitialized());
			TestEqual(TEXT("No old pawn Ready notification"), Fixture.Observer->ReadyCalls, 0);
			if (Mutation == 1)
			{
				TestNotNull(TEXT("Replacement configuration created its component"), Fixture.Node(TEXT("Replacement")));
				Fixture.Complete();
				TestTrue(TEXT("Replacement graph can initialize"), Fixture.Manager->IsPawnInitialized());
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPawnReadySubscriptionsTest, "Nelaric.GameplayRuntime.Pawn.ReadySubscriptions",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPawnReadySubscriptionsTest::RunTest(const FString& Parameters)
{
	using namespace Nelaric::Tests;
	FInitFixture Fixture({Entry(TEXT("First")), Entry(TEXT("Last"))});
	const FPawnInitializationCallback InitialReady = FInitFixture::ReadyCallback(Fixture.Observer.Get());
	Fixture.Manager->RegisterAndCallPawnInitialized(InitialReady);
	UInitRegressionComponent* First = Fixture.Node(TEXT("First"));
	First->bAllowReady = true;
	First->RequestInitRefresh();
	TestEqual(TEXT("Component readiness does not start pawn gameplay"), Fixture.Observer->ReadyCalls, 0);
	Fixture.Complete();
	TestEqual(TEXT("Duplicate registration does not duplicate broadcasts"), Fixture.Observer->ReadyCalls, 1);
	TStrongObjectPtr<UInitRegressionObserver> Late(NewObject<UInitRegressionObserver>());
	Fixture.Manager->RegisterPawnInitializationRevoked(FInitFixture::RevokedCallback(Late.Get()));
	Fixture.Manager->RegisterAndCallPawnInitialized(FInitFixture::ReadyCallback(Late.Get()));
	TestEqual(TEXT("Late subscriber receives current Ready immediately"), Late->ReadyCalls, 1);
	Fixture.Manager->UnregisterPawnInitializationCallback(InitialReady);
	Fixture.Manager->UnregisterPawnInitializationCallback(FInitFixture::RevokedCallback(Fixture.Observer.Get()));
	Fixture.Manager->InvalidatePawnContext();
	TestEqual(TEXT("Removed Ready listener is not called"), Fixture.Observer->ReadyCalls, 1);
	TestEqual(TEXT("Removed revoked listener is not called"), Fixture.Observer->RevokedCalls, 0);
	TestEqual(TEXT("Late listener observes one revocation"), Late->RevokedCalls, 1);
	TestEqual(TEXT("Late listener observes new Ready"), Late->ReadyCalls, 2);
	TestTrue(TEXT("Business observes Ready, revoked, Ready ordering"),
	         Late->Events == TArray<FName>{TEXT("Ready"), TEXT("Revoked"), TEXT("Ready")});
	UInitRegressionObserver* Dead = NewObject<UInitRegressionObserver>();
	const FPawnInitializationCallback DeadCallback = FInitFixture::ReadyCallback(Dead);
	Fixture.Manager->RegisterAndCallPawnInitialized(DeadCallback);
	Dead->MarkAsGarbage();
	TestFalse(TEXT("Destroyed UObject listener is unbound"), DeadCallback.IsBound());
	Fixture.Manager->InvalidatePawnContext();
	TestFalse(TEXT("Ready notifications always observe a Ready pawn"), Late->bInvalidReadyNotification);
	Fixture.Pawn->Destroy();
	TestFalse(TEXT("Pawn destruction stops subscribed gameplay"), Late->bGameplayActive);
	return true;
}

#endif
