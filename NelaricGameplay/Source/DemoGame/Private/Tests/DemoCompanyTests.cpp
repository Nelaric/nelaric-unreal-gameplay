// Copyright (c) 2026 Nelaric Contributors

#if WITH_DEV_AUTOMATION_TESTS
#include "AI/DemoCompanyCommandActor.h"
#include "AI/DemoCompanyRegistrySubsystem.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoCompanyRegistryTest, "Nelaric.Demo.Company.WorldAuthority",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDemoCompanyRegistryTest::RunTest(const FString& Parameters)
{
	const auto Values = UWorld::InitializationValues()
	                        .CreatePhysicsScene(false)
	                        .CreateNavigation(false)
	                        .CreateAISystem(false)
	                        .ShouldSimulatePhysics(false);
	UWorld* First =
	    UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	UWorld* Second =
	    UWorld::CreateWorld(EWorldType::PIE, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	auto* Registry = First->GetSubsystem<UDemoCompanyRegistrySubsystem>();
	auto* OtherRegistry = Second->GetSubsystem<UDemoCompanyRegistrySubsystem>();
	TestNotNull(TEXT("Game worlds create the registry"), Registry);
	TestNotNull(TEXT("Each world creates its own registry"), OtherRegistry);
	if (Registry && OtherRegistry)
	{
		auto* Company = First->SpawnActor<ADemoCompanyCommandActor>();
		auto* Duplicate = First->SpawnActor<ADemoCompanyCommandActor>();
		auto* Other = Second->SpawnActor<ADemoCompanyCommandActor>();
		const int32 Lease = Registry->RegisterCompany(Company, TEXT("Company"));
		TestTrue(TEXT("First publisher receives a lease"), Lease > 0);
		TestEqual(TEXT("Repeated registration is idempotent"), Registry->RegisterCompany(Company, TEXT("Company")),
		          Lease);
		AddExpectedError(TEXT("Duplicate company command rejected"), EAutomationExpectedErrorFlags::Contains, 1);
		TestEqual(TEXT("Duplicate cannot publish"), Registry->RegisterCompany(Duplicate, TEXT("Company")), 0);
		TestEqual(TEXT("A foreign-world actor cannot publish"), Registry->RegisterCompany(Other, TEXT("Other")), 0);
		TestTrue(TEXT("Second world has independent authority"),
		         OtherRegistry->RegisterCompany(Other, TEXT("Company")) > 0);
		TestNotEqual(TEXT("World callbacks have distinct run identities"), Registry->GetRunId(),
		             OtherRegistry->GetRunId());
		TestFalse(TEXT("Stale unregister cannot revoke authority"), Registry->UnregisterCompany(Company, Lease + 1));
		TestTrue(TEXT("Owner keeps authority after stale cancellation"),
		         Registry->HasPublicationAuthority(Company, Lease));
		TestTrue(TEXT("Exact unregister succeeds"), Registry->UnregisterCompany(Company, Lease));
		const int32 Next = Registry->RegisterCompany(Duplicate, TEXT("Company"));
		TestTrue(TEXT("Replacement receives a new lease"), Next > Lease);
		TestFalse(TEXT("Old callback cannot publish into the replacement"),
		          Registry->HasPublicationAuthority(Company, Lease));
	}
	First->DestroyWorld(false);
	Second->DestroyWorld(false);
	return true;
}
#endif
