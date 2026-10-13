#include "Game/KodWireTint.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FKodWireHpTintTest,
	"Kod.Hud.WireHpTint",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FKodWireHpTintTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	TestTrue(TEXT("100/100 green"), KodWireTint::FromHitPoints(100, 100).Equals(KodWireTint::Healthy()));
	TestTrue(TEXT("66/100 green"), KodWireTint::FromHitPoints(66, 100).Equals(KodWireTint::Healthy()));
	TestTrue(TEXT("65/100 yellow"), KodWireTint::FromHitPoints(65, 100).Equals(KodWireTint::Warning()));
	TestTrue(TEXT("33/100 yellow"), KodWireTint::FromHitPoints(33, 100).Equals(KodWireTint::Warning()));
	TestTrue(TEXT("32/100 red"), KodWireTint::FromHitPoints(32, 100).Equals(KodWireTint::Critical()));
	TestTrue(TEXT("0/100 red"), KodWireTint::FromHitPoints(0, 100).Equals(KodWireTint::Critical()));
	TestTrue(TEXT("0 max red"), KodWireTint::FromHitPoints(10, 0).Equals(KodWireTint::Critical()));
	TestTrue(TEXT("remain 0.66 green"), KodWireTint::FromRemain(0.66f).Equals(KodWireTint::Healthy()));
	TestTrue(TEXT("remain 0.50 yellow"), KodWireTint::FromRemain(0.50f).Equals(KodWireTint::Warning()));
	TestTrue(TEXT("remain 0.33 yellow"), KodWireTint::FromRemain(0.33f).Equals(KodWireTint::Warning()));
	TestTrue(TEXT("remain 0.32 red"), KodWireTint::FromRemain(0.32f).Equals(KodWireTint::Critical()));
	TestTrue(TEXT("green token"), KodWireTint::Healthy().Equals(FLinearColor(FColor(0x4C, 0xD9, 0x64))));
	TestTrue(TEXT("yellow token"), KodWireTint::Warning().Equals(FLinearColor(FColor(0xF2, 0xC2, 0x30))));
	TestTrue(TEXT("red token"), KodWireTint::Critical().Equals(FLinearColor(FColor(0xE5, 0x38, 0x3B))));
	TestTrue(TEXT("no shield rim"), FMath::IsNearlyZero(KodWireTint::ShieldRimStrength(0, 100)));
	TestTrue(TEXT("negative shield rim"), FMath::IsNearlyZero(KodWireTint::ShieldRimStrength(-3, 100)));
	TestTrue(TEXT("half shield rim"), FMath::IsNearlyEqual(KodWireTint::ShieldRimStrength(50, 100), 0.5f));
	TestTrue(TEXT("full shield rim"), FMath::IsNearlyEqual(KodWireTint::ShieldRimStrength(100, 100), 1.f));
	TestTrue(TEXT("shield without max"), FMath::IsNearlyEqual(KodWireTint::ShieldRimStrength(10, 0), 1.f));

	TArray<KodWireTint::FZone> Zones;
	KodWireTint::BuildZones(false, Zones);
	TestEqual(TEXT("unit zone count"), Zones.Num(), 4);
	TestTrue(TEXT("unit head"), Zones[0].Id == KodWireTint::EZone::Head);
	TestTrue(TEXT("unit torso"), Zones[1].Id == KodWireTint::EZone::Torso);
	TestTrue(TEXT("unit legs"), Zones[2].Id == KodWireTint::EZone::Legs);
	TestTrue(TEXT("unit weapon"), Zones[3].Id == KodWireTint::EZone::Weapon);
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
	TestTrue(TEXT("per-zone apply"), KodWireTint::ApplyPerZone(Zones, PerZone));
	TestTrue(TEXT("head green"), KodWireTint::FromRemain(Zones[0].Remain).Equals(KodWireTint::Healthy()));
	TestTrue(TEXT("torso yellow"), KodWireTint::FromRemain(Zones[1].Remain).Equals(KodWireTint::Warning()));
	TestTrue(TEXT("legs red"), KodWireTint::FromRemain(Zones[2].Remain).Equals(KodWireTint::Critical()));
	TestTrue(TEXT("weapon 0.66 green"), KodWireTint::FromRemain(Zones[3].Remain).Equals(KodWireTint::Healthy()));

	TArray<float> Short;
	Short.Add(1.f);
	const float Kept = Zones[0].Remain;
	TestFalse(TEXT("mismatched per-zone ignored"), KodWireTint::ApplyPerZone(Zones, Short));
	TestTrue(TEXT("mismatched leaves remain"), FMath::IsNearlyEqual(Zones[0].Remain, Kept));

	TArray<KodWireTint::FZone> Blocks;
	KodWireTint::BuildZones(true, Blocks);
	TestEqual(TEXT("building zone count"), Blocks.Num(), 3);
	TestTrue(TEXT("building roof"), Blocks[0].Id == KodWireTint::EZone::Roof);
	TestTrue(TEXT("building body"), Blocks[1].Id == KodWireTint::EZone::Body);
	TestTrue(TEXT("building base"), Blocks[2].Id == KodWireTint::EZone::Base);
	return true;
}
