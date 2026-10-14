#include "Game/KodHudLayout.h"
#include "Misc/AutomationTest.h"
#include "UObject/SoftObjectPath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodHudV3Test,
	"Kod.Hud.PaintedV3",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodHudV3Test::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TestEqual(TEXT("ten tabs"), FKodHudV3::TabCount(), 10);
	const FKodHudV3 Vanguard = FKodHudV3::ForFaction(false);
	const FKodHudV3 Scinder = FKodHudV3::ForFaction(true);
	TestEqual(TEXT("vanguard first tab"), Vanguard.Tab[0].X, 375.f);
	TestEqual(TEXT("vanguard last tab"), Vanguard.Tab[9].X, 989.f);
	TestEqual(TEXT("scinder first tab"), Scinder.Tab[0].X, 384.f);
	TestEqual(TEXT("scinder last tab"), Scinder.Tab[9].X, 998.f);
	auto Expect = [this](const FString& Path, const TCHAR* Stem)
	{
		const FSoftObjectPath Soft(Path);
		TestTrue(*FString::Printf(TEXT("path %s"), Stem), Soft.IsValid());
		TestTrue(*FString::Printf(TEXT("stem %s"), Stem), Path.Contains(Stem));
	};
	Expect(FKodHudV3::FramePath(false), TEXT("T_vg_hud_frame"));
	Expect(FKodHudV3::FramePath(true), TEXT("T_sc_hud_frame"));
	Expect(FKodHudV3::LightsPath(), TEXT("T_vg_lights"));
	Expect(FKodHudV3::BannerPath(false), TEXT("T_vg_achievement_banner"));
	Expect(FKodHudV3::BannerPath(true), TEXT("T_sc_achievement_banner"));
	Expect(FKodHudV3::ClockBoxPath(), TEXT("T_vg_clock_box"));
	Expect(FKodHudV3::StoryPath(false), TEXT("T_vg_story_window_2x"));
	Expect(FKodHudV3::StoryMaskPath(true), TEXT("T_sc_story_window_inner_mask_2x"));
	for (int32 Index = 0; Index < FKodHudV3::TabCount(); ++Index)
	{
		TestTrue(TEXT("vanguard tab width"), Vanguard.Tab[Index].W > 0.f);
		TestTrue(TEXT("scinder tab width"), Scinder.Tab[Index].W > 0.f);
		const FSoftObjectPath Assigned(FKodHudV3::TabPath(false, Index, true));
		const FSoftObjectPath Empty(FKodHudV3::TabPath(true, Index, false));
		TestTrue(TEXT("assigned tab path"), Assigned.IsValid());
		TestTrue(TEXT("empty tab path"), Empty.IsValid());
	}
	TestTrue(TEXT("third vanguard tab is 3"), FKodHudV3::TabPath(false, 2, true).Contains(TEXT("T_vg_numtab_3_assigned")));
	TestTrue(TEXT("zero key is the last tab"), FKodHudV3::TabPath(true, 9, false).Contains(TEXT("T_sc_numtab_0_empty")));
	return true;
}
