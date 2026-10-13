#include "Game/KodSkirmishSetup.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodSkirmishSetupTest,
	"Kod.Lobby.Setup",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodSkirmishSetupTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FKodSkirmishSetup Setup;
	Setup.Mode = EKodMatchMode::OfflineSkirmish;
	Setup.MapId = TEXT("Faultline");
	Setup.PlayerFaction = EKodRosterFaction::Random;
	Setup.AiFaction = EKodRosterFaction::Scinder;
	Setup.Difficulty = EKodLobbyDifficulty::Hard;
	Setup.PlayerColor = 2;
	Setup.AiColor = 4;
	Setup.PlayerStart = 1;
	Setup.AiStart = 0;
	Setup.bEnableAi = true;
	Setup.Seed = 7;

	FKodSkirmishSetup Parsed;
	TestTrue(TEXT("round trip"), FKodSkirmishSetup::TryParse(Setup.ToOptions(), Parsed));
	TestTrue(TEXT("mode"), Parsed.Mode == EKodMatchMode::OfflineSkirmish);
	TestTrue(TEXT("map"), Parsed.MapId == FName(TEXT("Faultline")));
	TestTrue(TEXT("player faction"), Parsed.PlayerFaction == EKodRosterFaction::Random);
	TestTrue(TEXT("ai faction"), Parsed.AiFaction == EKodRosterFaction::Scinder);
	TestTrue(TEXT("difficulty"), Parsed.Difficulty == EKodLobbyDifficulty::Hard);
	TestEqual(TEXT("ai step"), FKodSkirmishSetup::ToAiDifficulty(Parsed.Difficulty), 3);
	TestEqual(TEXT("normal step"), FKodSkirmishSetup::ToAiDifficulty(EKodLobbyDifficulty::Normal), 2);
	TestEqual(TEXT("easy step"), FKodSkirmishSetup::ToAiDifficulty(EKodLobbyDifficulty::Easy), 1);
	FKodSkirmishSetup Medium;
	TestTrue(TEXT("medium alias"), FKodSkirmishSetup::TryParse(TEXT("?Match=skirmish?Difficulty=Medium"), Medium));
	TestTrue(TEXT("medium is normal"), Medium.Difficulty == EKodLobbyDifficulty::Normal);
	TestEqual(TEXT("medium step"), FKodSkirmishSetup::ToAiDifficulty(Medium.Difficulty), 2);
	TestEqual(TEXT("player color"), Parsed.PlayerColor, 2);
	TestEqual(TEXT("ai color"), Parsed.AiColor, 4);
	TestEqual(TEXT("player start"), Parsed.PlayerStart, 1);
	TestEqual(TEXT("ai start"), Parsed.AiStart, 0);
	TestEqual(TEXT("seed"), Parsed.Seed, 7);
	TestTrue(TEXT("skirmish cheats"), FKodSkirmishSetup::MatchAllowsCheats(EKodMatchMode::OfflineSkirmish));
	TestFalse(TEXT("ranked cheats"), FKodSkirmishSetup::MatchAllowsCheats(EKodMatchMode::OnlineRanked));

	const int32 FirstRoll = Parsed.ResolvePlayerFactionIndex();
	const int32 SecondRoll = Parsed.ResolvePlayerFactionIndex();
	TestEqual(TEXT("random stable"), FirstRoll, SecondRoll);
	TestTrue(TEXT("random is a house"), FirstRoll == 0 || FirstRoll == 1);

	FKodSkirmishSetup Vanguard;
	Vanguard.PlayerFaction = EKodRosterFaction::Vanguard;
	TestEqual(TEXT("vanguard index"), Vanguard.ResolvePlayerFactionIndex(), 0);
	Vanguard.PlayerFaction = EKodRosterFaction::Scinder;
	TestEqual(TEXT("scinder index"), Vanguard.ResolvePlayerFactionIndex(), 1);

	EKodRosterFaction Dynasty = EKodRosterFaction::Vanguard;
	TestFalse(TEXT("dynasty rejected"), FKodSkirmishSetup::TryParseFaction(TEXT("Dynasty"), Dynasty));
	TestFalse(TEXT("dynasty selectable"), FKodSkirmishSetup::IsSelectableFaction(EKodRosterFaction::Dynasty));

	FKodSkirmishSetup Fallback;
	TestTrue(TEXT("dynasty option falls back"), FKodSkirmishSetup::TryParse(TEXT("?Match=skirmish?PFaction=Dynasty"), Fallback));
	TestTrue(TEXT("dynasty becomes vanguard"), Fallback.PlayerFaction == EKodRosterFaction::Vanguard);

	FKodSkirmishSetup SameStart;
	TestTrue(TEXT("shared start parses"), FKodSkirmishSetup::TryParse(TEXT("?Match=skirmish?PStart=0?AStart=0"), SameStart));
	TestNotEqual(TEXT("starts differ"), SameStart.PlayerStart, SameStart.AiStart);

	TestFalse(TEXT("empty options"), FKodSkirmishSetup::TryParse(TEXT(""), Parsed));

	TArray<FKodLobbyMap> Maps;
	FKodSkirmishSetup::GetMaps(Maps);
	bool bFaultline = false;
	bool bCoreLoopBlocked = false;
	for (const FKodLobbyMap& Map : Maps)
	{
		if (Map.Id == FName(TEXT("Faultline")))
		{
			bFaultline = Map.bPlayable;
		}
		if (Map.Id == FName(TEXT("CoreLoop")))
		{
			bCoreLoopBlocked = !Map.bPlayable;
		}
	}
	TestTrue(TEXT("faultline playable"), bFaultline);
	TestTrue(TEXT("core loop not playable"), bCoreLoopBlocked);

	FKodSkirmishSetup Campaign;
	TestTrue(TEXT("campaign"), FKodSkirmishSetup::TryParse(TEXT("?Match=campaign?Map=Faultline"), Campaign));
	TestTrue(TEXT("campaign mode"), Campaign.Mode == EKodMatchMode::Campaign);
	TestTrue(TEXT("campaign cheats stay with the match rule"), FKodSkirmishSetup::MatchAllowsCheats(Campaign.Mode));
	return true;
}
