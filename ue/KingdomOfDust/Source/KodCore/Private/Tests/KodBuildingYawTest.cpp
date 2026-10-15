#include "Game/KodBuildingYaw.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodBuildingYawTest,
	"Kod.Build.Yaw",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodBuildingYawTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const float Yaw = KodBuildingYaw::OffsetDegrees();
	TestTrue(TEXT("default yaw is 45"), FMath::IsNearlyEqual(Yaw, 45.f, 0.01f));
	TestTrue(TEXT("presentation adds the offset"), FMath::IsNearlyEqual(KodBuildingYaw::PresentationYaw(0.f), Yaw, 0.01f));
	TestTrue(TEXT("no shrink when the mesh is square to the grid"), FMath::IsNearlyEqual(KodBuildingYaw::ContainScale(0.f), 1.f, 0.001f));
	TestTrue(TEXT("quarter turn stays square"), FMath::IsNearlyEqual(KodBuildingYaw::ContainScale(90.f), 1.f, 0.001f));
	TestTrue(TEXT("45 degrees fits the square footprint"), FMath::IsNearlyEqual(KodBuildingYaw::ContainScale(45.f), 0.70710678f, 0.001f));
	const float Shown = KodBuildingYaw::PresentationYaw(10.f);
	TestTrue(TEXT("row yaw is kept"), FMath::IsNearlyEqual(Shown, Yaw + 10.f, 0.01f));
	return true;
}
