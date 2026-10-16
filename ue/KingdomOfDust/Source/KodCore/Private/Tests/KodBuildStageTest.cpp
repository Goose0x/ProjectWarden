#include "Game/KodBuildStage.h"
#include "Misc/AutomationTest.h"
#include "Sim/KodProductionTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodBuildStageTest,
	"Kod.Build.Stage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodBuildStageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FKodStructureState Fresh;
	Fresh.bComplete = false;
	Fresh.BuildTimeTotal = 80;
	Fresh.BuildTicksDone = 0;
	TestTrue(TEXT("new site is empty"), FMath::IsNearlyEqual(Fresh.BuildProgress(), 0.f));

	Fresh.BuildTicksDone = 40;
	TestTrue(TEXT("half the ticks is half the progress"), FMath::IsNearlyEqual(Fresh.BuildProgress(), 0.5f));

	Fresh.bComplete = true;
	TestTrue(TEXT("complete is finished"), FMath::IsNearlyEqual(Fresh.BuildProgress(), 1.f));

	const FKodBuildStageView Start = KodBuildStage::FromProgress(0.f);
	TestEqual(TEXT("first third"), Start.StageIndex, 0);
	TestTrue(TEXT("reveal starts at the bottom"), FMath::IsNearlyEqual(Start.Reveal, 0.f));
	TestFalse(TEXT("first third is not the finished mesh"), Start.bFinal);

	const FKodBuildStageView Middle = KodBuildStage::FromProgress(0.5f);
	TestEqual(TEXT("second third"), Middle.StageIndex, 1);
	TestTrue(TEXT("halfway remaps inside the stage"), FMath::IsNearlyEqual(Middle.Reveal, 0.5f));

	const FKodBuildStageView Late = KodBuildStage::FromProgress(1.f / 3.f);
	TestEqual(TEXT("boundary opens the next stage"), Late.StageIndex, 1);
	TestTrue(TEXT("boundary reveal starts over"), FMath::IsNearlyEqual(Late.Reveal, 0.f, 0.001f));

	const FKodBuildStageView Done = KodBuildStage::FromProgress(1.f);
	TestEqual(TEXT("finished index"), Done.StageIndex, 3);
	TestTrue(TEXT("finished mesh"), Done.bFinal);

	TestTrue(TEXT("geometry collection is exempt"), KodBuildStage::IsExemptClassName(TEXT("GeometryCollectionComponent")));
	TestTrue(TEXT("destructible is exempt"), KodBuildStage::IsExemptClassName(TEXT("DestructibleComponent")));
	TestFalse(TEXT("static mesh still builds"), KodBuildStage::IsExemptClassName(TEXT("StaticMeshComponent")));
	return true;
}
