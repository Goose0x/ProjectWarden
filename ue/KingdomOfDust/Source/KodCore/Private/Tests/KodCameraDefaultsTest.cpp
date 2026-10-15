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
	TestTrue(TEXT("distance"), Near(KodCameraSettings::DistanceSc2, 34.f));
	TestTrue(TEXT("distance min"), Near(KodCameraSettings::DistanceMinSc2, 16.f));
	TestTrue(TEXT("target z"), Near(KodCameraSettings::TargetZSc2, 0.f));
	TestTrue(TEXT("near"), Near(KodCameraSettings::NearSc2, 0.1f));
	TestTrue(TEXT("far"), Near(KodCameraSettings::FarSc2, 150.f));
	TestTrue(TEXT("unit scale"), Near(KodCameraSettings::UnitScaleUU, static_cast<float>(KodBuildGrid::CellUU)));
	TestTrue(TEXT("depot span"), Near(KodCameraSettings::DepotSpanUU(), 400.f));
	TestTrue(TEXT("distance uu"), Near(KodCameraSettings::ToUU(34.f), 6800.f));
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
	TestTrue(TEXT("tunable distance"), Near(Defaults.DistanceSc2, 34.f));
	const float Arm = KodCameraSettings::ToUU(KodCameraSettings::DistanceSc2);
	const float HqFraction = KodCameraSettings::ScreenWidthFraction(
		KodCameraSettings::HqSpanUU(),
		Arm,
		KodCameraSettings::FovDegrees,
		16.f / 9.f);
	TestTrue(TEXT("HQ is about 13% of the view width at 34"), HqFraction >= 0.125f && HqFraction <= 0.140f);
	TestTrue(TEXT("depot is 40% of the HQ"), Near(KodCameraSettings::DepotSpanUU() / KodCameraSettings::HqSpanUU(), 0.4f));
	const float WorkerPx = KodCameraSettings::BodyPixels(
		KodCameraSettings::WorkerBodyUU, Arm, KodCameraSettings::FovDegrees, KodCameraSettings::PitchDegrees, 720.f);
	const float MarinePx = KodCameraSettings::BodyPixels(
		KodCameraSettings::MarineBodyUU, Arm, KodCameraSettings::FovDegrees, KodCameraSettings::PitchDegrees, 720.f);
	TestTrue(TEXT("worker is 21-25 px at 720p"), WorkerPx >= 21.f && WorkerPx <= 25.f);
	TestTrue(TEXT("marine is 25-29 px at 720p"), MarinePx >= 25.f && MarinePx <= 29.f);
	const float Band = (1080.f - KodHudLayout::HudBandTop) / 1080.f;
	TestTrue(TEXT("HUD band is a quarter of the screen"), FMath::IsNearlyEqual(Band, 0.25f, 0.01f));
	const float Strip = KodHudLayout::ResourceStripH / 1080.f;
	TestTrue(TEXT("resource strip is 3-5% of height"), Strip >= 0.03f && Strip <= 0.05f);
	TestEqual(TEXT("command card columns"), KodHudLayout::CommandCols, 5);
	TestEqual(TEXT("command card rows"), KodHudLayout::CommandRows, 3);
	TestTrue(TEXT("minimap is square"), FMath::IsNearlyEqual(KodHudLayout::FrameMinimapW, KodHudLayout::FrameMinimapH, 0.01f));
	TestTrue(TEXT("tunable min"), Near(Defaults.DistanceMinSc2, 16.f));
	TestTrue(TEXT("scale log"), Defaults.DescribeScale().Contains(TEXT("UnitScale=200")));

	TestTrue(TEXT("bump stays on the plane"), FMath::IsNearlyEqual(KodCameraSettings::HoldPlaneZ(100.f, 112.f, KodCameraSettings::PlaneHoldUU), 100.f));
	TestTrue(TEXT("dip stays on the plane"), FMath::IsNearlyEqual(KodCameraSettings::HoldPlaneZ(100.f, 70.f, KodCameraSettings::PlaneHoldUU), 100.f));
	TestTrue(TEXT("plateau steps the plane"), FMath::IsNearlyEqual(KodCameraSettings::HoldPlaneZ(100.f, 400.f, KodCameraSettings::PlaneHoldUU), 400.f));
	float PanZ = 0.f;
	const float Bumps[] = { 4.f, -12.f, 18.f, -6.f, 9.f };
	for (float Bump : Bumps)
	{
		PanZ = KodCameraSettings::HoldPlaneZ(PanZ, Bump, KodCameraSettings::PlaneHoldUU);
	}
	TestTrue(TEXT("pan over uneven ground holds Z"), FMath::IsNearlyEqual(PanZ, 0.f));

	const FVector EdgeSpeed(4800.f, 0.f, 0.f);
	float Travel30 = 0.f;
	float Travel120 = 0.f;
	FVector Velocity30 = FVector::ZeroVector;
	FVector Velocity120 = FVector::ZeroVector;
	for (int32 Frame = 0; Frame < 30; ++Frame)
	{
		Velocity30 = KodCameraSettings::IntegratePanVelocity(Velocity30, EdgeSpeed, 1.f / 30.f, 0.f, true);
		Travel30 += Velocity30.X * (1.f / 30.f);
	}
	for (int32 Frame = 0; Frame < 120; ++Frame)
	{
		Velocity120 = KodCameraSettings::IntegratePanVelocity(Velocity120, EdgeSpeed, 1.f / 120.f, 0.f, true);
		Travel120 += Velocity120.X * (1.f / 120.f);
	}
	TestTrue(TEXT("edge pan is the same at 30 and 120"), FMath::IsNearlyEqual(Travel30, Travel120, 0.5f));
	TestTrue(TEXT("edge pan travels one second of speed"), FMath::IsNearlyEqual(Travel30, 4800.f, 0.5f));
	const FVector Released = KodCameraSettings::IntegratePanVelocity(EdgeSpeed, FVector::ZeroVector, 1.f / 60.f, 100000.f, false);
	TestTrue(TEXT("release does not coast"), Released.IsNearlyZero());
	const FVector Step = KodCameraSettings::IntegratePanVelocity(FVector::ZeroVector, EdgeSpeed, 1.f, 1000.f, false);
	TestTrue(TEXT("keyboard accel does not pass the target"), Step.X <= EdgeSpeed.X && Step.X > 0.f);
	return true;
}
