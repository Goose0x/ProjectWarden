#include "Game/KodCameraSettings.h"
#include "Game/KodHudLayout.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodBuildTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodCameraDefaultsTest,
	"Kod.Camera.Defaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

namespace
{
	bool Near(float Actual, float Expected)
	{
		return FMath::IsNearlyEqual(Actual, Expected, 0.01f);
	}
}

bool FKodCameraDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(TEXT("pitch"), Near(KodCameraSettings::PitchDegrees, -56.f));
	TestTrue(TEXT("yaw"), Near(KodCameraSettings::YawDegrees, 90.f));
	TestTrue(TEXT("roll"), Near(KodCameraSettings::RollDegrees, 0.f));
	TestTrue(TEXT("fov"), Near(KodCameraSettings::FovDegrees, 35.f));
	TestTrue(TEXT("distance"), Near(KodCameraSettings::DistanceSc2, 28.f));
	TestTrue(TEXT("distance min"), Near(KodCameraSettings::DistanceMinSc2, 16.f));
	TestTrue(TEXT("target z"), Near(KodCameraSettings::TargetZSc2, 0.f));
	TestTrue(TEXT("near"), Near(KodCameraSettings::NearSc2, 0.1f));
	TestTrue(TEXT("far"), Near(KodCameraSettings::FarSc2, 150.f));
	TestTrue(TEXT("unit scale"), Near(KodCameraSettings::UnitScaleUU, static_cast<float>(KodBuildGrid::CellUU)));
	TestTrue(TEXT("depot span"), Near(KodCameraSettings::DepotSpanUU(), 400.f));
	TestTrue(TEXT("distance uu"), Near(KodCameraSettings::ToUU(28.f), 5600.f));
	TestTrue(TEXT("min uu"), Near(KodCameraSettings::ToUU(16.f), 3200.f));
	TestTrue(TEXT("near uu"), Near(KodCameraSettings::ToUU(0.1f), 20.f));
	TestTrue(TEXT("far uu"), Near(KodCameraSettings::ToUU(150.f), 30000.f));
	float ResolvedNear = 0.f;
	float ResolvedFar = 0.f;
	KodCameraSettings::ResolveClipUU(0.1f, 150.f, 200.f, ResolvedNear, ResolvedFar);
	TestTrue(TEXT("resolved near uu"), Near(ResolvedNear, 20.f));
	TestTrue(TEXT("resolved far uu"), Near(ResolvedFar, 30000.f));
	TestTrue(TEXT("far is not raw sc2 units"), ResolvedFar > 1000.f);
	TestTrue(TEXT("resting camera is inside the far plane"), KodCameraSettings::ToUU(34.f) < ResolvedFar);

	TestTrue(TEXT("clamp below min"), Near(KodCameraSettings::ClampGameplayDistanceSc2(10.f, 16.f, 34.f), 16.f));
	TestTrue(TEXT("clamp inside"), Near(KodCameraSettings::ClampGameplayDistanceSc2(20.f, 16.f, 34.f), 20.f));
	TestTrue(TEXT("clamp above lock"), Near(KodCameraSettings::ClampGameplayDistanceSc2(40.f, 16.f, 34.f), 34.f));
	TestTrue(TEXT("ease hold"), Near(KodCameraSettings::EaseDistanceSc2(16.f, 34.f, 0.f), 16.f));
	TestTrue(TEXT("ease mid"), Near(KodCameraSettings::EaseDistanceSc2(16.f, 34.f, 0.5f), 25.f));
	TestTrue(TEXT("ease done"), Near(KodCameraSettings::EaseDistanceSc2(16.f, 34.f, 1.f), 34.f));

	const FKodCameraTunables Defaults;
	TestTrue(TEXT("tunable distance"), Near(Defaults.DistanceSc2, 28.f));
	const float Arm = KodCameraSettings::ToUU(KodCameraSettings::DistanceSc2);
	const float HqFraction = KodCameraSettings::ScreenWidthFraction(
		KodCameraSettings::HqSpanUU(),
		Arm,
		KodCameraSettings::FovDegrees,
		16.f / 9.f);
	TestTrue(TEXT("HQ is 15-17% of the view width"), HqFraction >= 0.15f && HqFraction <= 0.17f);
	TestTrue(TEXT("depot is 40% of the HQ"), Near(KodCameraSettings::DepotSpanUU() / KodCameraSettings::HqSpanUU(), 0.4f));
	const float WorkerPx = KodCameraSettings::BodyPixels(
		KodCameraSettings::WorkerBodyUU, Arm, KodCameraSettings::FovDegrees, KodCameraSettings::PitchDegrees, 720.f);
	const float MarinePx = KodCameraSettings::BodyPixels(
		KodCameraSettings::MarineBodyUU, Arm, KodCameraSettings::FovDegrees, KodCameraSettings::PitchDegrees, 720.f);
	TestTrue(TEXT("worker is 25-30 px at 720p"), WorkerPx >= 25.f && WorkerPx <= 30.f);
	TestTrue(TEXT("marine is 30-35 px at 720p"), MarinePx >= 30.f && MarinePx <= 35.f);
	const float Band = (1080.f - KodHudLayout::HudBandTop) / 1080.f;
	TestTrue(TEXT("HUD band is a quarter of the screen"), FMath::IsNearlyEqual(Band, 0.25f, 0.01f));
	const float Strip = KodHudLayout::ResourceStripH / 1080.f;
	TestTrue(TEXT("resource strip is 3-5% of height"), Strip >= 0.03f && Strip <= 0.05f);
	TestEqual(TEXT("command card columns"), KodHudLayout::CommandCols, 5);
	TestEqual(TEXT("command card rows"), KodHudLayout::CommandRows, 3);
	TestTrue(TEXT("minimap is square"), FMath::IsNearlyEqual(KodHudLayout::FrameMinimapW, KodHudLayout::FrameMinimapH, 0.01f));
	TestTrue(TEXT("tunable min"), Near(Defaults.DistanceMinSc2, 16.f));
	TestTrue(TEXT("scale log"), Defaults.DescribeScale().Contains(TEXT("UnitScale=200")));
	return true;
}
