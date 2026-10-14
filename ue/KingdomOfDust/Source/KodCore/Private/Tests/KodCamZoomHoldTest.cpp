#include "Engine/World.h"
#include "Game/KodRTSCameraPawn.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodCamZoomHoldTest,
	"Kod.Cam.ZoomHolds",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodCamZoomHoldTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodCamZoom_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodCamZoom")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);
	AKodRTSCameraPawn* CameraPawn = World->SpawnActor<AKodRTSCameraPawn>(AKodRTSCameraPawn::StaticClass(), FTransform::Identity);
	TestNotNull(TEXT("camera"), CameraPawn);
	if (!CameraPawn)
	{
		World->DestroyWorld(false);
		return false;
	}
	const float Step = FMath::Max(1.f, CameraPawn->ZoomStep);
	const float Notches = (6800.f - 3200.f) / Step;
	CameraPawn->Zoom(Notches);
	const float Dt = 1.f / 60.f;
	const int32 Frames = FMath::CeilToInt(2.f / Dt);
	for (int32 Frame = 0; Frame < Frames; ++Frame)
	{
		CameraPawn->Tick(Dt);
	}
	TestTrue(TEXT("zoom holds at the minimum"), FMath::IsNearlyEqual(CameraPawn->GetCurrentArmLength(), 3200.f, 1.f));
	World->DestroyWorld(false);
	return true;
}
