#include "Game/KodBuildVisual.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "Warden/KodWardenPaths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodBuildVisualTest,
	"Kod.Build.Visual",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodBuildVisualTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	IConsoleVariable* Stage2 = IConsoleManager::Get().FindConsoleVariable(TEXT("kod.Build.Stage2"));
	IConsoleVariable* Stage3 = IConsoleManager::Get().FindConsoleVariable(TEXT("kod.Build.Stage3"));
	IConsoleVariable* Ghost = IConsoleManager::Get().FindConsoleVariable(TEXT("kod.Build.DynastyGhost"));
	TestNotNull(TEXT("stage2 cvar"), Stage2);
	TestNotNull(TEXT("stage3 cvar"), Stage3);
	TestNotNull(TEXT("dynasty cvar"), Ghost);
	if (!Stage2 || !Stage3 || !Ghost)
	{
		return false;
	}
	const float Saved2 = Stage2->GetFloat();
	const float Saved3 = Stage3->GetFloat();
	const int32 SavedGhost = Ghost->GetInt();
	Stage2->Set(0.33f, ECVF_SetByCode);
	Stage3->Set(0.66f, ECVF_SetByCode);
	Ghost->Set(0, ECVF_SetByCode);

	TestEqual(TEXT("start is stage 1"), KodBuildVisual::StageIndex(0.f), 0);
	TestEqual(TEXT("just under 33% stays stage 1"), KodBuildVisual::StageIndex(0.329f), 0);
	TestEqual(TEXT("33% is stage 2"), KodBuildVisual::StageIndex(0.33f), 1);
	TestEqual(TEXT("just under 66% stays stage 2"), KodBuildVisual::StageIndex(0.659f), 1);
	TestEqual(TEXT("66% is stage 3"), KodBuildVisual::StageIndex(0.66f), 2);
	TestEqual(TEXT("finished is the final mesh"), KodBuildVisual::StageIndex(1.f), 3);

	const FName Hall(KodWardenPaths::Id_CommandCenter);
	const FName Bunker(KodWardenPaths::Id_Bunker);
	const FName Outpost(TEXT("KodEnemyOutpost"));
	TestTrue(TEXT("command center is slice"), KodBuildVisual::IsSliceBuilding(Hall));
	TestTrue(TEXT("bunker is slice"), KodBuildVisual::IsSliceBuilding(Bunker));
	TestTrue(TEXT("scinder hq is slice"), KodBuildVisual::IsSliceBuilding(FName(TEXT("ScinderHQ"))));
	TestFalse(TEXT("enemy outpost is not slice"), KodBuildVisual::IsSliceBuilding(Outpost));

	TestEqual(TEXT("slice stages"), static_cast<int32>(KodBuildVisual::Resolve(Hall, EKodBuildVisual::Stages, true)), static_cast<int32>(EKodBuildVisual::Stages));
	TestEqual(TEXT("missing stage meshes fall back"), static_cast<int32>(KodBuildVisual::Resolve(Hall, EKodBuildVisual::Stages, false)), static_cast<int32>(EKodBuildVisual::Scaffold));
	TestEqual(TEXT("non-slice stages fall back"), static_cast<int32>(KodBuildVisual::Resolve(Outpost, EKodBuildVisual::Stages, true)), static_cast<int32>(EKodBuildVisual::Scaffold));
	TestEqual(TEXT("vfx stays vfx"), static_cast<int32>(KodBuildVisual::Resolve(Outpost, EKodBuildVisual::Vfx, false)), static_cast<int32>(EKodBuildVisual::Vfx));
	TestEqual(TEXT("scaffold stays scaffold"), static_cast<int32>(KodBuildVisual::Resolve(Bunker, EKodBuildVisual::Scaffold, true)), static_cast<int32>(EKodBuildVisual::Scaffold));
	TestFalse(TEXT("dynasty ghost defaults off"), KodBuildVisual::DynastyGhostEnabled());

	TestTrue(TEXT("dry fill"), FMath::IsNearlyEqual(KodBuildVisual::StageFillLevel(0), 0.f));
	TestTrue(TEXT("puddle fill"), FMath::IsNearlyEqual(KodBuildVisual::StageFillLevel(1), 0.33f));
	TestTrue(TEXT("near fill"), FMath::IsNearlyEqual(KodBuildVisual::StageFillLevel(2), 0.66f));
	TestTrue(TEXT("full fill"), FMath::IsNearlyEqual(KodBuildVisual::StageFillLevel(3), 1.f));
	TestTrue(TEXT("dry Z"), FMath::IsNearlyEqual(KodBuildVisual::PoolSurfaceZcm(0), -160.f));
	TestTrue(TEXT("puddle Z"), FMath::IsNearlyEqual(KodBuildVisual::PoolSurfaceZcm(1), -42.f));
	TestTrue(TEXT("near Z"), FMath::IsNearlyEqual(KodBuildVisual::PoolSurfaceZcm(2), -10.f));
	TestTrue(TEXT("full Z"), FMath::IsNearlyEqual(KodBuildVisual::PoolSurfaceZcm(3), 0.f));
	TestTrue(TEXT("pool Z starts at the old height"), FMath::IsNearlyEqual(KodBuildVisual::InterpPoolZ(-160.f, -42.f, 0.f), -160.f));
	TestTrue(TEXT("pool Z is halfway at 0.25s"), FMath::IsNearlyEqual(KodBuildVisual::InterpPoolZ(-160.f, -42.f, 0.25f), -101.f));
	TestTrue(TEXT("pool Z arrives in half a second"), FMath::IsNearlyEqual(KodBuildVisual::InterpPoolZ(-42.f, -10.f, 0.5f), -10.f));

	Stage2->Set(Saved2, ECVF_SetByCode);
	Stage3->Set(Saved3, ECVF_SetByCode);
	Ghost->Set(SavedGhost, ECVF_SetByCode);
	return true;
}
