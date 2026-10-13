#include "Game/KodHUD.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodShotAfterTest,
	"Kod.Hud.ShotAfter",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodShotAfterTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	float Seconds = -1.f;
	TestFalse(TEXT("absent"), AKodHUD::ParseShotAfterSeconds(TEXT("-game"), Seconds));
	TestTrue(TEXT("dashed"), AKodHUD::ParseShotAfterSeconds(TEXT("ProjectWarden -kod.ShotAfter=8 -log"), Seconds));
	TestEqual(TEXT("dashed seconds"), Seconds, 8.f);
	TestTrue(TEXT("plain"), AKodHUD::ParseShotAfterSeconds(TEXT("kod.ShotAfter=2.5"), Seconds));
	TestTrue(TEXT("plain value"), FMath::IsNearlyEqual(Seconds, 2.5f));
	TestTrue(TEXT("zero is immediate"), AKodHUD::ParseShotAfterSeconds(TEXT("-kod.ShotAfter=0"), Seconds));
	TestEqual(TEXT("zero seconds"), Seconds, 0.f);
	TestFalse(TEXT("negative rejected"), AKodHUD::ParseShotAfterSeconds(TEXT("-kod.ShotAfter=-3"), Seconds));
	return true;
}
