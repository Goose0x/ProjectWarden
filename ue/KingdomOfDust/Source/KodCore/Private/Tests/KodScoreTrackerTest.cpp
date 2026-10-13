#include "Sim/KodScoreTracker.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodScoreTrackerTest,
	"Kod.Score.Totals",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodScoreTrackerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FKodMatchScoreTracker Tracker;
	const FName Marine(TEXT("Ranger"));
	const FName Medic(TEXT("Medic"));
	const FName Barracks(TEXT("Barracks"));

	Tracker.NoteUnitTrained(0, Marine);
	Tracker.NoteUnitTrained(0, Marine);
	Tracker.NoteUnitTrained(0, Marine);
	Tracker.NoteUnitTrained(0, Medic);
	Tracker.NoteUnitLost(0, Marine);
	Tracker.NoteUnitLost(0, Marine);
	Tracker.NoteUnitKilled(1, Marine);
	Tracker.NoteUnitKilled(1, Marine);
	Tracker.NoteStructureBuilt(0, Barracks);
	Tracker.NoteStructureLost(0, Barracks);
	Tracker.NoteStructureRazed(1, Barracks);
	Tracker.NoteGather(0, 100, 40);
	Tracker.NoteSpend(0, 50, 10);
	Tracker.NoteGather(1, 20, 0);
	for (int32 Step = 0; Step < 30; ++Step)
	{
		Tracker.NoteCommand(0);
	}
	Tracker.NoteCommand(1);

	const FKodScorePlayer* You = Tracker.FindPlayer(0);
	const FKodScorePlayer* Opponent = Tracker.FindPlayer(1);
	TestTrue(TEXT("both columns"), You != nullptr && Opponent != nullptr);
	if (!You || !Opponent)
	{
		return false;
	}

	TestEqual(TEXT("trained"), You->UnitsTrained, 4);
	TestEqual(TEXT("lost"), You->UnitsLost, 2);
	TestEqual(TEXT("ai killed"), Opponent->UnitsKilled, 2);
	TestEqual(TEXT("built"), You->StructuresBuilt, 1);
	TestEqual(TEXT("structures lost"), You->StructuresLost, 1);
	TestEqual(TEXT("razed"), Opponent->StructuresRazed, 1);
	TestEqual(TEXT("jadeite gathered"), You->JadeiteCollected, 100);
	TestEqual(TEXT("luminene gathered"), You->LumineneCollected, 40);
	TestEqual(TEXT("jadeite spent"), You->JadeiteSpent, 50);
	TestEqual(TEXT("luminene spent"), You->LumineneSpent, 10);
	TestEqual(TEXT("ai jadeite"), Opponent->JadeiteCollected, 20);
	TestEqual(TEXT("commands"), You->Commands, 30);
	TestEqual(TEXT("apm over one minute"), Tracker.AverageApm(0, 960), 30);
	TestEqual(TEXT("zero tick apm"), Tracker.AverageApm(0, 0), 0);

	bool bMarine = false;
	bool bMedic = false;
	for (const FKodScoreTypeRow& Row : You->Units)
	{
		if (Row.Id == Marine)
		{
			bMarine = Row.Trained == 3 && Row.Lost == 2 && Row.Killed == 0;
		}
		if (Row.Id == Medic)
		{
			bMedic = Row.Trained == 1 && Row.Lost == 0;
		}
	}
	TestTrue(TEXT("marine row"), bMarine);
	TestTrue(TEXT("medic row"), bMedic);

	bool bBarracksBuilt = false;
	for (const FKodScoreTypeRow& Row : You->Structures)
	{
		if (Row.Id == Barracks)
		{
			bBarracksBuilt = Row.Trained == 1 && Row.Lost == 1;
		}
	}
	TestTrue(TEXT("barracks row"), bBarracksBuilt);

	Tracker.ConsiderSample(FKodMatchScoreTracker::SampleTicks, 0, 8, 400, 12, 15);
	Tracker.ConsiderSample(FKodMatchScoreTracker::SampleTicks, 0, 9, 999, 1, 1);
	You = Tracker.FindPlayer(0);
	TestTrue(TEXT("one sample"), You && You->Samples.Num() == 1);
	if (!You || You->Samples.Num() != 1)
	{
		return false;
	}
	TestEqual(TEXT("sample workers"), You->Samples[0].Workers, 8);
	TestEqual(TEXT("sample army"), You->Samples[0].ArmyValue, 400);
	TestEqual(TEXT("sample supply"), You->Samples[0].SupplyUsed, 12);
	TestEqual(TEXT("sample cap"), You->Samples[0].SupplyCap, 15);
	TestEqual(TEXT("sample commands"), You->Samples[0].Commands, 30);
	TestEqual(TEXT("sample tick"), You->Samples[0].Tick, FKodMatchScoreTracker::SampleTicks);
	TestEqual(TEXT("peak from first window"), Tracker.PeakApm(0), 180);

	Tracker.NoteGather(0, 100, 0);
	Tracker.ConsiderSample(FKodMatchScoreTracker::SampleTicks * 2, 0, 8, 400, 12, 15);
	You = Tracker.FindPlayer(0);
	TestTrue(TEXT("second sample"), You && You->Samples.Num() == 2);
	if (!You || You->Samples.Num() != 2)
	{
		return false;
	}
	const int32 JadeiteRate = FKodMatchScoreTracker::PerMinute(
		You->Samples[1].JadeiteCollected - You->Samples[0].JadeiteCollected,
		You->Samples[1].Tick - You->Samples[0].Tick);
	TestEqual(TEXT("jadeite per minute"), JadeiteRate, 600);
	TestEqual(TEXT("gathered total"), You->JadeiteCollected, 200);

	TestTrue(TEXT("other team ignored"), Tracker.FindPlayer(2) == nullptr);
	Tracker.NoteUnitTrained(3, Marine);
	TestEqual(TEXT("trained stays"), Tracker.FindPlayer(0)->UnitsTrained, 4);

	Tracker.SetTeamColor(0, 9);
	Tracker.SetTeamColor(1, 4);
	TestEqual(TEXT("color clamp"), Tracker.FindPlayer(0)->ColorIndex, 5);
	TestEqual(TEXT("ai color"), Tracker.FindPlayer(1)->ColorIndex, 4);
	return true;
}
