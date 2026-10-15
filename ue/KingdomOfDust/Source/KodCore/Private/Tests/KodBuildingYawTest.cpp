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
	TestTrue(TEXT("default yaw is 0"), FMath::IsNearlyEqual(Yaw, 0.f, 0.01f));
	TestTrue(TEXT("presentation adds the offset"), FMath::IsNearlyEqual(KodBuildingYaw::PresentationYaw(0.f), Yaw, 0.01f));
	const float Shown = KodBuildingYaw::PresentationYaw(10.f);
	TestTrue(TEXT("row yaw is kept"), FMath::IsNearlyEqual(Shown, Yaw + 10.f, 0.01f));
	return true;
}
