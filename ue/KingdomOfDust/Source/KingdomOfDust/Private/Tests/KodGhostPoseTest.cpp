#include "Game/KodLanding.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Slice0/KodBuildGhost.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodGhostPoseTest,
	"Kod.Build.GhostPose",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodGhostPoseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodGhost_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodGhost")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);
	const FKodLandingPose Pose = KodLanding::MakePose(3, 1, 2, 2, 80.f);
	AKodBuildGhost* Ghost = World->SpawnActor<AKodBuildGhost>();
	TestNotNull(TEXT("ghost"), Ghost);
	if (Ghost)
	{
		TArray<uint8> Cells;
		Cells.Init(1, Pose.FootX * Pose.FootY);
		Ghost->Show(FName(TEXT("SupplyDepot")), Pose, Cells, true);
		TestTrue(TEXT("ghost matches the canonical placement"),
			KodLanding::SameTransform(Ghost->GetActorLocation(), Ghost->GetActorRotation().Yaw, Pose.Location, Pose.Yaw));
	}
	World->DestroyWorld(false);
	return true;
}
