#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Game/KodGameMode.h"
#include "Game/KodHUD.h"
#include "Game/KodPlayerController.h"
#include "Game/KodRTSCameraPawn.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodNoFadeAfterFactionTest,
	"Kod.Play.NoFadeAfterFaction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodNoFadeAfterFactionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestFalse(TEXT("painted lights stay off the frame"), AKodHUD::DrawsFullScreenLights());

	const FString PackageName = FString::Printf(TEXT("/Temp/KodNoFade_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodNoFade")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);

	AKodGameMode* Mode = World->SpawnActor<AKodGameMode>();
	AKodPlayerController* PC = World->SpawnActor<AKodPlayerController>();
	AKodRTSCameraPawn* CameraPawn = World->SpawnActor<AKodRTSCameraPawn>();
	AKodHUD* HUD = World->SpawnActor<AKodHUD>();
	APlayerCameraManager* Manager = World->SpawnActor<APlayerCameraManager>();
	TestNotNull(TEXT("mode"), Mode);
	TestNotNull(TEXT("controller"), PC);
	TestNotNull(TEXT("camera"), CameraPawn);
	TestNotNull(TEXT("hud"), HUD);
	TestNotNull(TEXT("camera manager"), Manager);
	if (!Mode || !PC || !CameraPawn || !HUD || !Manager)
	{
		World->DestroyWorld(false);
		return false;
	}
	PC->Possess(CameraPawn);
	PC->MyHUD = HUD;
	Manager->InitializeFor(PC);
	PC->PlayerCameraManager = Manager;
	const FLinearColor Orange(1.f, 154.f / 255.f, 48.f / 255.f, 1.f);
	Manager->SetManualCameraFade(1.f, Orange, false);
	HUD->SetFullScreenOverlayAlpha(1.f);
	TestTrue(TEXT("fade starts covered"), Manager->FadeAmount > 0.9f);
	TestTrue(TEXT("overlay starts covered"), HUD->GetFullScreenOverlayAlpha() > 0.9f);

	Mode->ApplyStartFaction(0);
	TestTrue(TEXT("fade cleared on apply"), FMath::IsNearlyZero(Manager->FadeAmount));
	TestFalse(TEXT("fade disabled on apply"), Manager->bEnableFading);
	TestTrue(TEXT("overlay cleared on apply"), FMath::IsNearlyZero(HUD->GetFullScreenOverlayAlpha()));

	for (int32 Step = 0; Step < 10; ++Step)
	{
		Manager->UpdateCamera(0.1f);
	}
	TestTrue(TEXT("no camera fade 1s after apply"), FMath::IsNearlyZero(Manager->FadeAmount));
	TestFalse(TEXT("fade stays off 1s after apply"), Manager->bEnableFading);
	TestTrue(TEXT("no full-screen overlay 1s after apply"), FMath::IsNearlyZero(HUD->GetFullScreenOverlayAlpha()));

	World->DestroyWorld(false);
	return true;
}
