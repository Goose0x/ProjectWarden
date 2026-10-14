#include "Game/KodSkirmishSetup.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodMatchStartTest,
	"Kod.Play.FactionStart",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodMatchStartTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FKodMatchStart Pie = FKodSkirmishSetup::ResolveMatchStart(TEXT("-game"), FString());
	TestTrue(TEXT("pie prompts"), Pie.bPrompt);
	TestEqual(TEXT("pie defaults vanguard"), Pie.FactionIndex, 0);

	const FKodMatchStart Shot = FKodSkirmishSetup::ResolveMatchStart(TEXT("-kod.ShotAfter=8 -kod.Faction=Scinder"), FString());
	TestFalse(TEXT("shot skips the prompt"), Shot.bPrompt);
	TestEqual(TEXT("shot faction"), Shot.FactionIndex, 1);

	const FKodMatchStart Auto = FKodSkirmishSetup::ResolveMatchStart(TEXT("-kod.Autoplay=1"), FString());
	TestFalse(TEXT("autoplay skips the prompt"), Auto.bPrompt);
	TestEqual(TEXT("autoplay defaults vanguard"), Auto.FactionIndex, 0);

	const FKodMatchStart Lobby = FKodSkirmishSetup::ResolveMatchStart(TEXT(""), TEXT("?Match=skirmish?PFaction=Scinder"));
	TestFalse(TEXT("front end skips the prompt"), Lobby.bPrompt);
	TestEqual(TEXT("front end faction"), Lobby.FactionIndex, 1);
	return true;
}
