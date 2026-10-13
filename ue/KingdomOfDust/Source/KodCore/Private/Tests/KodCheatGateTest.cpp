#include "Game/KodSkirmishSetup.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodCheatGateTest,
	"Kod.Cheat.OfflineOnly",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodCheatGateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(TEXT("campaign offline"), FKodSkirmishSetup::CheatsAllowed(EKodMatchMode::Campaign, false));
	TestTrue(TEXT("skirmish offline"), FKodSkirmishSetup::CheatsAllowed(EKodMatchMode::OfflineSkirmish, false));
	TestFalse(TEXT("ranked"), FKodSkirmishSetup::CheatsAllowed(EKodMatchMode::OnlineRanked, false));
	TestFalse(TEXT("replay"), FKodSkirmishSetup::CheatsAllowed(EKodMatchMode::Replay, false));
	TestFalse(TEXT("custom"), FKodSkirmishSetup::CheatsAllowed(EKodMatchMode::Custom, false));
	TestFalse(TEXT("campaign online"), FKodSkirmishSetup::CheatsAllowed(EKodMatchMode::Campaign, true));
	TestFalse(TEXT("skirmish online"), FKodSkirmishSetup::CheatsAllowed(EKodMatchMode::OfflineSkirmish, true));
	TestFalse(TEXT("ranked online"), FKodSkirmishSetup::CheatsAllowed(EKodMatchMode::OnlineRanked, true));

	TestTrue(TEXT("match ranked"), FKodSkirmishSetup::OptionsMarkOnline(TEXT("?Match=ranked")));
	TestTrue(TEXT("match online"), FKodSkirmishSetup::OptionsMarkOnline(TEXT("?Match=online")));
	TestTrue(TEXT("online flag"), FKodSkirmishSetup::OptionsMarkOnline(TEXT("?Match=skirmish?Online=1")));
	TestTrue(TEXT("session ranked"), FKodSkirmishSetup::OptionsMarkOnline(TEXT("?Match=skirmish?Session=ranked")));
	TestFalse(TEXT("plain skirmish"), FKodSkirmishSetup::OptionsMarkOnline(TEXT("?Match=skirmish")));
	TestFalse(TEXT("plain campaign"), FKodSkirmishSetup::OptionsMarkOnline(TEXT("?Match=campaign")));

	const FString Refusal(FKodSkirmishSetup::CheatRefusalText());
	TestEqual(TEXT("refusal"), Refusal, FString(TEXT("Cheats are not available in online or ranked play.")));

	const bool bRankedBlocked = !FKodSkirmishSetup::CheatsAllowed(
		EKodMatchMode::OnlineRanked,
		FKodSkirmishSetup::OptionsMarkOnline(TEXT("?Match=ranked")));
	const bool bFlaggedSkirmishBlocked = !FKodSkirmishSetup::CheatsAllowed(
		EKodMatchMode::OfflineSkirmish,
		FKodSkirmishSetup::OptionsMarkOnline(TEXT("?Match=skirmish?Online=1")));
	TestTrue(TEXT("ranked options block"), bRankedBlocked);
	TestTrue(TEXT("online flag blocks skirmish"), bFlaggedSkirmishBlocked);
	return true;
}
