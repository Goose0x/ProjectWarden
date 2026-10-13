#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Slice0/KodFaultline.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodFaultlineMarkerTest,
	"Kod.Faultline.LanePoints",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodFaultlineMarkerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString Path = FPaths::Combine(FPaths::ProjectConfigDir(), TEXT("KodDress/faultline_training/map_features.json"));
	FString Json;
	TestTrue(TEXT("map_features.json loads"), FFileHelper::LoadFileToString(Json, *Path));
	if (Json.IsEmpty())
	{
		return false;
	}

	auto Expect = [this, &Json](const TCHAR* Id, float WantX, float WantY)
	{
		float X = 0.f;
		float Y = 0.f;
		const bool bFound = KodFaultline::ReadMarkerPoint(Json, Id, X, Y);
		TestTrue(FString::Printf(TEXT("%s found"), Id), bFound);
		TestTrue(FString::Printf(TEXT("%s is not the origin"), Id), FMath::Abs(X) > 1.f || FMath::Abs(Y) > 1.f);
		TestTrue(FString::Printf(TEXT("%s x"), Id), FMath::IsNearlyEqual(X, WantX, 1.f));
		TestTrue(FString::Printf(TEXT("%s y"), Id), FMath::IsNearlyEqual(Y, WantY, 1.f));
	};

	Expect(TEXT("QRF_LANE_01_WP00"), 1500.f, 2300.f);
	Expect(TEXT("QRF_LANE_01_WP09"), -5500.f, -5500.f);
	Expect(TEXT("SPAWN_CW_01"), 1500.f, 2300.f);
	return true;
}

#endif
