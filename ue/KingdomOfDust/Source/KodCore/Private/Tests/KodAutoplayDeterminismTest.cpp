#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodSimSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodAutoplayDeterminismTest,
	"Kod.Soak.AutoplayDeterminism",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodAutoplayDeterminismTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodAutoplay_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodAutoplay")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);
	UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>();
	TestNotNull(TEXT("sim"), Sim);
	if (!Sim)
	{
		World->DestroyWorld(false);
		return false;
	}

	const int32 Ticks = 24;
	const bool bSame = Sim->RunAutoplayDeterminismCheck(Ticks);
	TestTrue(TEXT("two autoplay runs hash equal"), bSame);
	World->DestroyWorld(false);
	return true;
}
