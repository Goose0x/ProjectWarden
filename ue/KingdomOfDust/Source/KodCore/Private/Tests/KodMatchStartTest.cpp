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

	const FKodMatchStart Zoom = FKodSkirmishSetup::ResolveMatchStart(TEXT("-kod.ShotZoom=1 -kod.Faction=VG"), FString());
	TestFalse(TEXT("zoom shot skips the prompt"), Zoom.bPrompt);
	TestEqual(TEXT("zoom shot vanguard"), Zoom.FactionIndex, 0);

	const FKodMatchStart Buildings = FKodSkirmishSetup::ResolveMatchStart(TEXT("-kod.ShotBuildings=1 -kod.Faction=SC"), FString());
	TestFalse(TEXT("building shot skips the prompt"), Buildings.bPrompt);
	TestEqual(TEXT("building shot scinder"), Buildings.FactionIndex, 1);

	const FKodMatchStart Select = FKodSkirmishSetup::ResolveMatchStart(TEXT("-kod.ShotSelectMove=1 -kod.Faction=SC"), FString());
	TestFalse(TEXT("select shot skips the prompt"), Select.bPrompt);
	TestEqual(TEXT("select shot scinder"), Select.FactionIndex, 1);

	const FKodMatchStart Pan = FKodSkirmishSetup::ResolveMatchStart(TEXT("-kod.ShotPan=1 -kod.Faction=VG"), FString());
	TestFalse(TEXT("pan shot skips the prompt"), Pan.bPrompt);
	const FKodMatchStart Voice = FKodSkirmishSetup::ResolveMatchStart(TEXT("-kod.ShotVoiceSpam=1 -kod.Faction=VG"), FString());
	TestFalse(TEXT("voice shot skips the prompt"), Voice.bPrompt);
	const FKodMatchStart Rite = FKodSkirmishSetup::ResolveMatchStart(TEXT("-kod.ShotRiteWell=1 -kod.Faction=SC"), FString());
	TestFalse(TEXT("rite well shot skips the prompt"), Rite.bPrompt);
	TestEqual(TEXT("rite well shot scinder"), Rite.FactionIndex, 1);
	return true;
}
