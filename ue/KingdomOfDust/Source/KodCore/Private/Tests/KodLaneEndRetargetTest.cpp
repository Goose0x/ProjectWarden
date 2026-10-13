#include "Misc/AutomationTest.h"
#include "Sim/KodSimSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodLaneEndRetargetTest,
	"Kod.Faultline.LaneEndRetarget",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodLaneEndRetargetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	int32 Id = 0;
	float X = 0.f;
	float Y = 0.f;

	TestFalse(TEXT("still on the lane"), UKodSimSubsystem::FaultlineLaneEndRetarget(
		false, false, -5500.f, -5500.f, 4, 1500.f, 2300.f, 2, -7500.f, -7500.f, Id, X, Y));
	TestFalse(TEXT("already in contact"), UKodSimSubsystem::FaultlineLaneEndRetarget(
		true, true, -5500.f, -5500.f, 4, 1500.f, 2300.f, 2, -7500.f, -7500.f, Id, X, Y));

	TestTrue(TEXT("lane end picks the command center"), UKodSimSubsystem::FaultlineLaneEndRetarget(
		true, false, -5500.f, -5500.f, 4, 1500.f, 2300.f, 2, -7500.f, -7500.f, Id, X, Y));
	TestEqual(TEXT("command center id"), Id, 2);
	TestTrue(TEXT("command center x"), FMath::IsNearlyEqual(X, -7500.f, 1.f));
	TestTrue(TEXT("command center y"), FMath::IsNearlyEqual(Y, -7500.f, 1.f));

	TestTrue(TEXT("lane end picks a nearer marine"), UKodSimSubsystem::FaultlineLaneEndRetarget(
		true, false, -5500.f, -5500.f, 9, -5400.f, -5400.f, 2, -7500.f, -7500.f, Id, X, Y));
	TestEqual(TEXT("marine id"), Id, 9);
	return true;
}
