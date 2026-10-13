#include "Game/KodWireTint.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodWireHpTintTest,
	"Kod.Hud.WireHpTint",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodWireHpTintTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(TEXT("100/100 green"), KodWireTint::FromHitPoints(100, 100).Equals(KodWireTint::Healthy()));
	TestTrue(TEXT("67/100 green"), KodWireTint::FromHitPoints(67, 100).Equals(KodWireTint::Healthy()));
	TestTrue(TEXT("66/100 yellow"), KodWireTint::FromHitPoints(66, 100).Equals(KodWireTint::Warning()));
	TestTrue(TEXT("33/100 yellow"), KodWireTint::FromHitPoints(33, 100).Equals(KodWireTint::Warning()));
	TestTrue(TEXT("32/100 red"), KodWireTint::FromHitPoints(32, 100).Equals(KodWireTint::Critical()));
	TestTrue(TEXT("0/100 red"), KodWireTint::FromHitPoints(0, 100).Equals(KodWireTint::Critical()));
	TestTrue(TEXT("0 max red"), KodWireTint::FromHitPoints(10, 0).Equals(KodWireTint::Critical()));
	TestTrue(TEXT("healthy band"), KodWireTint::BandFor(100, 100) == KodWireTint::EBand::Healthy);
	TestTrue(TEXT("warning band"), KodWireTint::BandFor(66, 100) == KodWireTint::EBand::Warning);
	TestTrue(TEXT("critical band"), KodWireTint::BandFor(32, 100) == KodWireTint::EBand::Critical);

	TArray<KodWireTint::FZone> Zones;
	KodWireTint::BuildZones(false, Zones);
	TestEqual(TEXT("infantry zone count"), Zones.Num(), 6);
	KodWireTint::ApplyOverallHp(Zones, 10, 100);
	for (const KodWireTint::FZone& Zone : Zones)
	{
		TestTrue(TEXT("overall 10% red"), KodWireTint::FromRemain(Zone.Remain).Equals(KodWireTint::Critical()));
	}

	TArray<float> PerZone;
	PerZone.Add(1.f);
	PerZone.Add(0.5f);
	PerZone.Add(0.f);
	PerZone.Add(0.66f);
	PerZone.Add(0.33f);
	PerZone.Add(0.32f);
	TestTrue(TEXT("per-zone apply"), KodWireTint::ApplyPerZone(Zones, PerZone));
	TestTrue(TEXT("zone full green"), KodWireTint::FromRemain(Zones[0].Remain).Equals(KodWireTint::Healthy()));
	TestTrue(TEXT("zone half yellow"), KodWireTint::FromRemain(Zones[1].Remain).Equals(KodWireTint::Warning()));
	TestTrue(TEXT("zone empty red"), KodWireTint::FromRemain(Zones[2].Remain).Equals(KodWireTint::Critical()));
	TestTrue(TEXT("zone 0.66 yellow"), KodWireTint::FromRemain(Zones[3].Remain).Equals(KodWireTint::Warning()));
	TestTrue(TEXT("zone 0.33 yellow"), KodWireTint::FromRemain(Zones[4].Remain).Equals(KodWireTint::Warning()));
	TestTrue(TEXT("zone 0.32 red"), KodWireTint::FromRemain(Zones[5].Remain).Equals(KodWireTint::Critical()));

	TArray<float> Short;
	Short.Add(1.f);
	const float Kept = Zones[0].Remain;
	TestFalse(TEXT("mismatched per-zone ignored"), KodWireTint::ApplyPerZone(Zones, Short));
	TestTrue(TEXT("mismatched leaves remain"), FMath::IsNearlyEqual(Zones[0].Remain, Kept));

	TArray<KodWireTint::FZone> Blocks;
	KodWireTint::BuildZones(true, Blocks);
	TestEqual(TEXT("structure zone count"), Blocks.Num(), 4);
	return true;
}
