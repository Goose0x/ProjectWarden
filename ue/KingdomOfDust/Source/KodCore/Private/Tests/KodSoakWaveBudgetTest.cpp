#include "Misc/AutomationTest.h"
#include "Sim/KodSimSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodSoakWaveBudgetTest,
	"Kod.Soak.WaveAndMemory",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodSoakWaveBudgetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestFalse(TEXT("waiting on outposts"), UKodSimSubsystem::FaultlineWaveDue(0, 3, 10, 100, false));
	TestTrue(TEXT("outposts down"), UKodSimSubsystem::FaultlineWaveDue(0, 0, 10, 100, false));
	TestFalse(TEXT("autoplay early"), UKodSimSubsystem::FaultlineWaveDue(0, 3, 99, 100, true));
	TestTrue(TEXT("autoplay clock"), UKodSimSubsystem::FaultlineWaveDue(0, 3, 100, 100, true));
	TestFalse(TEXT("already scheduled"), UKodSimSubsystem::FaultlineWaveDue(1, 0, 200, 100, true));

	const uint64 Megabyte = 1024ull * 1024ull;
	TestFalse(TEXT("no elapsed"), UKodSimSubsystem::SoakMemoryOverBudget(1000, 1000 + 80ull * Megabyte, 0));
	TestFalse(TEXT("under 50MB in 5 min"), UKodSimSubsystem::SoakMemoryOverBudget(0, 40ull * Megabyte, 300));
	TestTrue(TEXT("over 50MB in 5 min"), UKodSimSubsystem::SoakMemoryOverBudget(0, 51ull * Megabyte, 300));

	const int32 FlatTimes[] = {120, 150, 180, 210, 240};
	const uint64 FlatBytes[] = {2000ull * Megabyte, 2001ull * Megabyte, 2000ull * Megabyte, 2002ull * Megabyte, 2001ull * Megabyte};
	TestFalse(TEXT("flat slope"), UKodSimSubsystem::SoakMemorySlopeOverBudget(FlatTimes, FlatBytes, 5));

	const int32 ShortTimes[] = {120, 150};
	const uint64 ShortBytes[] = {1000ull * Megabyte, 1100ull * Megabyte};
	TestFalse(TEXT("window too short"), UKodSimSubsystem::SoakMemorySlopeOverBudget(ShortTimes, ShortBytes, 2));

	const int32 LeakTimes[] = {120, 150, 180, 210, 240};
	uint64 LeakBytes[5];
	for (int32 Index = 0; Index < 5; ++Index)
	{
		LeakBytes[Index] = 1900ull * Megabyte + static_cast<uint64>(Index) * 14ull * Megabyte;
	}
	TestTrue(TEXT("14MB per 30s slope"), UKodSimSubsystem::SoakMemorySlopeOverBudget(LeakTimes, LeakBytes, 5));
	return true;
}
