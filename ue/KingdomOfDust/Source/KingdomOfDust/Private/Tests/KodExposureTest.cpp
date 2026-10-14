#include "Components/DirectionalLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Slice0/KodSlice0GameMode.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodExposureTest,
	"Kod.Play.Exposure",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodExposureTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString PackageName = FString::Printf(TEXT("/Temp/KodExposure_%s"), *FGuid::NewGuid().ToString());
	UPackage* Package = CreatePackage(*PackageName);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("KodExposure")), Package, true);
	TestNotNull(TEXT("world"), World);
	if (!World)
	{
		return false;
	}
	FURL Url;
	World->InitializeActorsForPlay(Url);

	ADirectionalLight* Sun = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(FRotator(-37.f, 150.f, 0.f)));
	TestNotNull(TEXT("sun"), Sun);
	UDirectionalLightComponent* SunComponent = Sun ? Cast<UDirectionalLightComponent>(Sun->GetLightComponent()) : nullptr;
	TestNotNull(TEXT("sun component"), SunComponent);
	if (SunComponent)
	{
		SunComponent->SetIntensity(10.f);
	}
	World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform(FRotator(-20.f, 10.f, 0.f)));
	const int32 Enabled = AKodSlice0GameMode::KeepSingleDirectionalSun(World, true);
	TestEqual(TEXT("one directional light"), Enabled, 1);
	if (SunComponent)
	{
		TestTrue(TEXT("sun stays at the authored 10"), FMath::IsNearlyEqual(SunComponent->Intensity, 10.f, 0.05f));
	}

	AKodSlice0GameMode::ApplyMatchExposure(World);
	IConsoleVariable* AutoExposure = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DefaultFeature.AutoExposure"));
	IConsoleVariable* PreExposure = IConsoleManager::Get().FindConsoleVariable(TEXT("r.EyeAdaptation.PreExposureOverride"));
	TestNotNull(TEXT("auto exposure cvar"), AutoExposure);
	TestNotNull(TEXT("pre-exposure cvar"), PreExposure);
	if (AutoExposure)
	{
		TestEqual(TEXT("auto exposure off"), AutoExposure->GetInt(), 0);
	}
	if (PreExposure)
	{
		TestTrue(TEXT("no pre-exposure override"), FMath::IsNearlyZero(PreExposure->GetFloat()));
	}

	bool bManual = false;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		if (!It->bUnbound || !It->bEnabled)
		{
			continue;
		}
		if (It->Settings.bOverride_AutoExposureMethod && It->Settings.AutoExposureMethod == AEM_Manual)
		{
			bManual = true;
			TestTrue(TEXT("min brightness not locked"), !It->Settings.bOverride_AutoExposureMinBrightness);
			TestTrue(TEXT("max brightness not locked"), !It->Settings.bOverride_AutoExposureMaxBrightness);
			TestTrue(TEXT("physical camera exposure off"), It->Settings.bOverride_AutoExposureApplyPhysicalCameraExposure && !It->Settings.AutoExposureApplyPhysicalCameraExposure);
		}
	}
	TestTrue(TEXT("manual exposure volume"), bManual);

	World->DestroyWorld(false);
	return true;
}
