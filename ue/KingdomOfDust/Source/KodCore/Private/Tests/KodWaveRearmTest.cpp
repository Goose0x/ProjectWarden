#include "Misc/AutomationTest.h"
#include "Sim/KodSimSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodWaveRearmTest,
	"Kod.Faultline.WaveRearm",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodWaveRearmTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestFalse(TEXT("army alive holds"), UKodSimSubsystem::FaultlineWaveShouldRetreat(285, 285, 6, 16));
	TestTrue(TEXT("dead army ends the wave"), UKodSimSubsystem::FaultlineWaveShouldRetreat(285, 285, 0, 16));
	TestTrue(TEXT("broken wave still ends"), UKodSimSubsystem::FaultlineWaveShouldRetreat(80, 285, 6, 16));
	TestFalse(TEXT("short of 90s"), UKodSimSubsystem::FaultlineWaveShouldRetreat(285, 285, 6, 90 * 16 - 1));
	TestTrue(TEXT("90s ends the wave"), UKodSimSubsystem::FaultlineWaveShouldRetreat(285, 285, 6, 90 * 16));

	const int64 Ended = 1000;
	const int32 Delay = 16 * 45;
	TestFalse(TEXT("rearm waits out the gap"), UKodSimSubsystem::FaultlineWaveRearmDue(Ended + Delay - 1, Ended, Delay));
	TestTrue(TEXT("rearm after the gap"), UKodSimSubsystem::FaultlineWaveRearmDue(Ended + Delay, Ended, Delay));
	TestTrue(TEXT("dead army then rearm"), UKodSimSubsystem::FaultlineWaveShouldRetreat(285, 285, 0, 16)
		&& UKodSimSubsystem::FaultlineWaveRearmDue(Ended + Delay, Ended, Delay));
	return true;
}
