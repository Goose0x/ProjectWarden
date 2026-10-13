#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodProductionTypes.h"
#include "Sim/KodSimSubsystem.h"
#include "Warden/KodWardenPaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodSimBlobV12Test,
	"Kod.Sim.BlobV12",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodSimBlobV12Test::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodBlobV12_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodBlobV12")), Package, true);
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

	FKodFaultlineScript Script;
	Script.OutpostCount = 1;
	Script.SpawnX = 4000;
	Script.SpawnY = -2200;
	Script.HomeX = -6000;
	Script.HomeY = -5800;
	Script.DelayTicks = 16;
	Script.RetreatTicks = 32;
	Sim->ArmFaultlineScript(Script);
	TArray<FVector> Lane;
	Lane.Add(FVector(1200.f, -800.f, 0.f));
	Lane.Add(FVector(-2400.f, -1600.f, 0.f));
	Sim->SetFaultlineLane(Lane);
	TestEqual(TEXT("lane armed"), Sim->GetFaultlineLaneCount(), 2);

	const TArray<uint8> V12 = Sim->ExportSimBlobAtVersion(12);
	const TArray<uint8> V13 = Sim->ExportSimBlobAtVersion(KodWardenPaths::SimSaveVersion);
	TestTrue(TEXT("v12 blob has a header"), V12.Num() >= static_cast<int32>(sizeof(int32)));
	int32 Header = 0;
	if (V12.Num() >= static_cast<int32>(sizeof(int32)))
	{
		FMemory::Memcpy(&Header, V12.GetData(), sizeof(int32));
	}
	TestEqual(TEXT("blob version is 12"), Header, 12);
	TestEqual(TEXT("v13 is the focus id longer"), V13.Num(), V12.Num() + static_cast<int32>(sizeof(int32)));

	TArray<uint8> WithTrailingFocus = V12;
	const int32 FakeFocus = 4242;
	WithTrailingFocus.Append(reinterpret_cast<const uint8*>(&FakeFocus), sizeof(FakeFocus));
	Sim->ApplySimBlob(WithTrailingFocus);
	TestTrue(TEXT("v12 load keeps the tutorial"), Sim->IsFaultlineTraining());
	TestEqual(TEXT("v12 load keeps both lane points"), Sim->GetFaultlineLaneCount(), 2);
	TestEqual(TEXT("v12 load does not read a focus id"), Sim->GetFaultlineWaveFocusId(), 0);

	World->DestroyWorld(false);
	return true;
}
