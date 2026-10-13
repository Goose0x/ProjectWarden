#include "Game/KodHudProposed.h"
#include "Game/KodWireTint.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodHudProposedTest,
	"Kod.Hud.ProposedBands",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodHudProposedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	UKodHudProposed* Style = NewObject<UKodHudProposed>();
	TestNotNull(TEXT("defaults"), Style);
	if (!Style)
	{
		return false;
	}
	TestEqual(TEXT("wire healthy stays 66%"), Style->WireHealthyPermille, 660);
	TestEqual(TEXT("wire warning stays 33%"), Style->WireWarningPermille, 330);
	TestEqual(TEXT("world healthy is 50%"), Style->WorldHealthyPermille, 500);
	TestEqual(TEXT("world warning is 25%"), Style->WorldWarningPermille, 250);
	TestEqual(TEXT("proposed build opacity"), Style->IncompleteOpacity, 0.55f);
	TestEqual(TEXT("proposed error hold"), Style->ErrorHoldSeconds, 2.f);
	TestEqual(TEXT("proposed error fade"), Style->ErrorFadeSeconds, 1.f);
	TestEqual(TEXT("proposed shield gap"), Style->ShieldGapPx, 2.f);

	TestEqual(
		TEXT("wire 66 is green"),
		static_cast<int32>(KodWireTint::BandForPermille(66, 100, Style->WireHealthyPermille, Style->WireWarningPermille, false)),
		static_cast<int32>(KodWireTint::EBand::Healthy));
	TestEqual(
		TEXT("wire 65 is yellow"),
		static_cast<int32>(KodWireTint::BandForPermille(65, 100, Style->WireHealthyPermille, Style->WireWarningPermille, false)),
		static_cast<int32>(KodWireTint::EBand::Warning));
	TestEqual(
		TEXT("world 50 is yellow"),
		static_cast<int32>(KodWireTint::BandForPermille(50, 100, Style->WorldHealthyPermille, Style->WorldWarningPermille, true)),
		static_cast<int32>(KodWireTint::EBand::Warning));
	TestEqual(
		TEXT("world 51 is green"),
		static_cast<int32>(KodWireTint::BandForPermille(51, 100, Style->WorldHealthyPermille, Style->WorldWarningPermille, true)),
		static_cast<int32>(KodWireTint::EBand::Healthy));
	TestEqual(
		TEXT("world 25 is yellow"),
		static_cast<int32>(KodWireTint::BandForPermille(25, 100, Style->WorldHealthyPermille, Style->WorldWarningPermille, true)),
		static_cast<int32>(KodWireTint::EBand::Warning));
	TestEqual(
		TEXT("world 24 is red"),
		static_cast<int32>(KodWireTint::BandForPermille(24, 100, Style->WorldHealthyPermille, Style->WorldWarningPermille, true)),
		static_cast<int32>(KodWireTint::EBand::Critical));
	return true;
}
