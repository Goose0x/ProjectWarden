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
	TestFalse(TEXT("no growth"), UKodSimSubsystem::SoakMemoryOverBudget(1000, 1000, 300));
	TestFalse(TEXT("under 50MB in 5 min"), UKodSimSubsystem::SoakMemoryOverBudget(0, 40ull * Megabyte, 300));
	TestTrue(TEXT("over 50MB in 5 min"), UKodSimSubsystem::SoakMemoryOverBudget(0, 51ull * Megabyte, 300));
	TestTrue(TEXT("over the 30s rate"), UKodSimSubsystem::SoakMemoryOverBudget(0, 8ull * Megabyte, 30));
	return true;
}
