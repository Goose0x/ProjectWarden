#include "Game/KodCameraSettings.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodBuildTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodCameraDefaultsTest,
	"Kod.Camera.Defaults",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

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

	TestTrue(TEXT("clamp below min"), Near(KodCameraSettings::ClampGameplayDistanceSc2(10.f, 16.f, 34.f), 16.f));
	TestTrue(TEXT("clamp inside"), Near(KodCameraSettings::ClampGameplayDistanceSc2(20.f, 16.f, 34.f), 20.f));
	TestTrue(TEXT("clamp above lock"), Near(KodCameraSettings::ClampGameplayDistanceSc2(40.f, 16.f, 34.f), 34.f));
	TestTrue(TEXT("ease hold"), Near(KodCameraSettings::EaseDistanceSc2(16.f, 34.f, 0.f), 16.f));
	TestTrue(TEXT("ease mid"), Near(KodCameraSettings::EaseDistanceSc2(16.f, 34.f, 0.5f), 25.f));
	TestTrue(TEXT("ease done"), Near(KodCameraSettings::EaseDistanceSc2(16.f, 34.f, 1.f), 34.f));

	const FKodCameraTunables Defaults;
	TestTrue(TEXT("tunable distance"), Near(Defaults.DistanceSc2, 34.f));
	TestTrue(TEXT("tunable min"), Near(Defaults.DistanceMinSc2, 16.f));
	TestTrue(TEXT("scale log"), Defaults.DescribeScale().Contains(TEXT("UnitScale=200")));
	return true;
}
