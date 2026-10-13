#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Sim/KodSimSubsystem.h"
#include "Slice0/KodMapLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodFaultlinePathTest,
	"Kod.Faultline.RampPath",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodFaultlinePathTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("KodDress/faultline_training/map_features.json"));
	FString Json;
	TestTrue(TEXT("map_features.json loads"), FFileHelper::LoadFileToString(Json, *Path));
	if (Json.IsEmpty())
	{
		return false;
	}
	TArray<int64> Blocked;
	KodMapLayout::BlockedCellsFromJson(Json, -56, Blocked);
	TestTrue(TEXT("cliffs loaded"), Blocked.Num() > 0);

	const FString PackageName = FString::Printf(TEXT("/Temp/KodRampPath_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodRampPath")), Package, true);
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
	Sim->SetCliffBlockCells(Blocked);

	auto CellToWorld = [](int32 CellX, int32 CellY)
	{
		return FVector((static_cast<float>(CellX) + 0.5f) * 200.f - 11200.f,
			(static_cast<float>(CellY) + 0.5f) * 200.f - 11200.f,
			80.f);
	};
	const FVector Wp00 = CellToWorld(63, 67);
	const FVector Wp09 = CellToWorld(28, 28);
	const FVector Spawn = CellToWorld(18, 18);
	const FVector OffPlateau = CellToWorld(48, 30);

	TestTrue(TEXT("WP09 reaches WP00"), Sim->HasMoveRoute(Wp09, Wp00, 0));
	TestTrue(TEXT("WP00 reaches WP09"), Sim->HasMoveRoute(Wp00, Wp09, 0));
	TestTrue(TEXT("P1_SPAWN leaves the main plateau"), Sim->HasMoveRoute(Spawn, OffPlateau, 0));

	World->DestroyWorld(false);
	return true;
}

#endif
