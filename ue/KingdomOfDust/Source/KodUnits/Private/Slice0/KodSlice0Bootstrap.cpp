#include "Slice0/KodSlice0Bootstrap.h"
#include "GameplayTagContainer.h"
#include "Slice0/KodDataCatalog.h"
#include "Data/KodAbilityDefinition.h"
#include "Data/KodWeaponDefinition.h"
#include "Data/KodUnitDefinition.h"
#include "Data/KodBuildingDefinition.h"
#include "Data/KodFactionDefinition.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Actors/KodUnit.h"
#include "Warden/KodWardenPaths.h"
#include "Sim/KodBuildTicks.h"
#include "Sim/KodUnitSheet.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

TWeakObjectPtr<UKodDataCatalog> UKodSlice0Bootstrap::GCatalog;

namespace
{
	// Shop sheet CooldownSeconds on the real uasset is 0.35.
	// The code path used when that asset is missing is 13 sim ticks (RoundToInt(0.8 * 16)).
	constexpr float RangerRifleDamage = 12.f;
	constexpr float RangerRifleRange = 900.f;
	constexpr int32 RangerRifleCooldownTicks = 13;
	constexpr float RangerRifleCooldownSeconds = static_cast<float>(RangerRifleCooldownTicks) * KodBuildTicks::SimDt;

	struct FRangerRifleStatCache
	{
		TWeakObjectPtr<UWorld> World;
		float Damage = 0.f;
		float Range = 0.f;
		float CooldownSeconds = 0.f;
		bool bValid = false;
	};

	FRangerRifleStatCache GRangerRifleStatCache;
}

static UObject* TryLoadWardenAsset(FName DefinitionId)
{
	const FSoftObjectPath Path = KodWardenPaths::MakeSoftPath(*DefinitionId.ToString());
	if (UObject* Existing = Path.ResolveObject())
	{
		return Existing;
	}
	// Missing DataAssets are made in BuildCatalog. TryLoad would log
	// "Failed to find object /Game/Warden/Data/<Id>" for each one.
	if (!FPackageName::DoesPackageExist(Path.GetLongPackageName()))
	{
		return nullptr;
	}
	return Path.TryLoad();
}

UKodDataCatalog* UKodSlice0Bootstrap::EnsureCatalog(UObject* Outer)
{
	if (UKodDataCatalog* Existing = GCatalog.Get())
	{
		return Existing;
	}
	return BuildCatalog(Outer);
}

UKodDataCatalog* UKodSlice0Bootstrap::BuildCatalog(UObject* Outer)
{
	UObject* CatalogOuter = Outer;
	if (!CatalogOuter)
	{
		CatalogOuter = GetTransientPackage();
	}

	UKodDataCatalog* Catalog = NewObject<UKodDataCatalog>(CatalogOuter, TEXT("KodSlice0Catalog"), RF_Transient);
	UKodWeaponDefinition* Rifle = MakeRangerRifle(Catalog);
	UKodUnitDefinition* Marine = MakeMarine(Catalog, Rifle);
	UKodUnitDefinition* ScinderInfantry = MakeScinderInfantry(Catalog, Rifle);
	UKodUnitDefinition* ScnFighter = MakeScnFighter(Catalog);
	UKodUnitDefinition* ScnTwinblade = MakeScnTwinblade(Catalog);
	UKodUnitDefinition* ScnWorker = MakeScnWorker(Catalog);
	UKodUnitDefinition* HideOx = MakeHideOx(Catalog);
	UKodUnitDefinition* Dozer = MakeDozer(Catalog);
	UKodBuildingDefinition* CC = MakeCommandCenter(Catalog, Dozer);
	UKodUnitDefinition* Medic = MakeMedic(Catalog);
	UKodBuildingDefinition* Barracks = MakeBarracks(Catalog, Marine);
	if (Barracks && Medic)
	{
		Barracks->TrainableUnits.Add(Medic);
	}
	UKodBuildingDefinition* Depot = MakeSupplyDepot(Catalog);
	UKodBuildingDefinition* Rig = MakeRig(Catalog);
	UKodBuildingDefinition* EngineeringBay = MakeEngineeringBay(Catalog);
	UKodBuildingDefinition* Bunker = MakeBunker(Catalog);
	UKodBuildingDefinition* Turret = MakeTurret(Catalog);
	UKodUnitDefinition* OrbitalGatherer = NewObject<UKodUnitDefinition>(Catalog, FName(KodWardenPaths::Id_OrbitalGatherer), RF_Public | RF_Transient);
	OrbitalGatherer->DefinitionId = FName(KodWardenPaths::Id_OrbitalGatherer);
	OrbitalGatherer->DisplayName = NSLOCTEXT("Kod", "Unit_OrbitalGatherer", "Orbital Gatherer");
	OrbitalGatherer->MaxHealth = 60.f;
	OrbitalGatherer->Armor = 0.f;
	OrbitalGatherer->MoveSpeed = 400.f;
	OrbitalGatherer->SightRadius = 1200.f;
	OrbitalGatherer->bCanGather = true;
	OrbitalGatherer->bBiological = true;
	OrbitalGatherer->Cost.Credits = 0;
	OrbitalGatherer->Cost.Supply = 0;
	OrbitalGatherer->Cost.BuildTime = 0;
	OrbitalGatherer->BuildTicks = 0;
	UKodUnitDefinition* IsrPlatform = NewObject<UKodUnitDefinition>(Catalog, FName(KodWardenPaths::Id_IsrPlatform), RF_Public | RF_Transient);
	IsrPlatform->DefinitionId = FName(KodWardenPaths::Id_IsrPlatform);
	IsrPlatform->DisplayName = NSLOCTEXT("Kod", "Unit_IsrPlatform", "ISR Platform");
	IsrPlatform->MaxHealth = 200.f;
	IsrPlatform->MoveSpeed = 180.f;
	IsrPlatform->SightRadius = 1200.f;
	IsrPlatform->bFlyer = true;
	IsrPlatform->Cost.Supply = 0;
	IsrPlatform->Cost.BuildTime = 0;
	IsrPlatform->BuildTicks = 0;
	UKodFactionDefinition* USA = MakeUSA(Catalog, CC, Dozer, Marine);
	UKodResourceNodeDefinition* JadeiteNode = MakeJadeiteNode(Catalog);
	UKodResourceNodeDefinition* LumineneVent = MakeLumineneVent(Catalog);

	Catalog->RegisterAsset(FName(KodWardenPaths::Id_RangerRifle), Rifle);
	if (Marine && Marine->Abilities.Num() > 0)
	{
		Catalog->RegisterAsset(FName(KodWardenPaths::Id_CombatStim), MakeCombatStim(Catalog));
	}
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Marine), Marine);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_ScinderInfantry), ScinderInfantry);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_ScnFighter), ScnFighter);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_ScnTwinblade), ScnTwinblade);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_ScnWorker), ScnWorker);
	if (HideOx)
	{
		Catalog->RegisterAsset(HideOx->DefinitionId, HideOx);
	}
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Dozer), Dozer);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_CommandCenter), CC);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Barracks), Barracks);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_SupplyDepot), Depot);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Rig), Rig);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Medic), Medic);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_EngineeringBay), EngineeringBay);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Bunker), Bunker);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Turret), Turret);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_OrbitalGatherer), OrbitalGatherer);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_IsrPlatform), IsrPlatform);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_USA), USA);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_JadeiteNode), JadeiteNode);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_LumineneVent), LumineneVent);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_OilSource), LumineneVent);

	GCatalog = Catalog;
	return Catalog;
}

UObject* UKodSlice0Bootstrap::ResolveDefinition(FName DefinitionId, UObject* Outer)
{
	if (DefinitionId.IsNone())
	{
		return nullptr;
	}
	if (DefinitionId == FName(KodWardenPaths::Id_Ranger))
	{
		DefinitionId = FName(KodWardenPaths::Id_Marine);
	}
	const FName MarineId(KodWardenPaths::Id_Marine);
	const FName LegacyId(KodWardenPaths::Id_Ranger);
	if (UObject* Loaded = TryLoadWardenAsset(DefinitionId))
	{
		if (UKodUnitDefinition* LoadedUnit = Cast<UKodUnitDefinition>(Loaded))
		{
			if (DefinitionId == MarineId && (LoadedUnit->DefinitionId.IsNone() || LoadedUnit->DefinitionId == LegacyId))
			{
				const FName CopyName(TEXT("MarineFromLegacyRanger"));
				if (UKodUnitDefinition* ExistingCopy = FindObject<UKodUnitDefinition>(GetTransientPackage(), *CopyName.ToString()))
				{
					return ExistingCopy;
				}
				UKodUnitDefinition* Copy = DuplicateObject<UKodUnitDefinition>(LoadedUnit, GetTransientPackage(), CopyName);
				if (Copy)
				{
					Copy->DefinitionId = MarineId;
					Copy->UnitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Unit.Marine")), false);
					Copy->SetFlags(RF_Transient);
					return Copy;
				}
			}
		}
		return Loaded;
	}
	UKodDataCatalog* Catalog = EnsureCatalog(Outer);
	return Catalog ? Catalog->FindAsset(DefinitionId) : nullptr;
}

UKodWeaponDefinition* UKodSlice0Bootstrap::ResolveWeapon(FName DefinitionId, UObject* Outer)
{
	return Cast<UKodWeaponDefinition>(ResolveDefinition(DefinitionId, Outer));
}

void UKodSlice0Bootstrap::ResolveRangerRifleCombatStats(float& OutDamage, float& OutRange, float& OutCooldownSeconds, UWorld* LogWorld)
{
	if (LogWorld && GRangerRifleStatCache.bValid && GRangerRifleStatCache.World.Get() == LogWorld)
	{
		OutDamage = GRangerRifleStatCache.Damage;
		OutRange = GRangerRifleStatCache.Range;
		OutCooldownSeconds = GRangerRifleStatCache.CooldownSeconds;
		return;
	}

	bool bFallback = true;
	float Damage = RangerRifleDamage;
	float Range = RangerRifleRange;
	float CooldownSeconds = RangerRifleCooldownSeconds;
	if (UObject* Loaded = TryLoadWardenAsset(FName(KodWardenPaths::Id_RangerRifle)))
	{
		if (const UKodWeaponDefinition* Weapon = Cast<UKodWeaponDefinition>(Loaded))
		{
			bFallback = false;
			Damage = Weapon->Damage;
			Range = Weapon->Range;
			CooldownSeconds = Weapon->CooldownSeconds;
		}
	}

	if (bFallback)
	{
		if (const UKodWeaponDefinition* CatalogWeapon = ResolveWeapon(FName(KodWardenPaths::Id_RangerRifle), GetTransientPackage()))
		{
			bFallback = false;
			Damage = CatalogWeapon->Damage;
			Range = CatalogWeapon->Range;
			CooldownSeconds = CatalogWeapon->CooldownSeconds;
		}
	}
	if (bFallback)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodWeapon fallback RangerRifle Damage=%.0f Range=%.0f Cooldown=%.4f"),
			Damage,
			Range,
			CooldownSeconds);
	}

	if (LogWorld)
	{
		GRangerRifleStatCache.World = LogWorld;
		GRangerRifleStatCache.bValid = true;
		GRangerRifleStatCache.Damage = Damage;
		GRangerRifleStatCache.Range = Range;
		GRangerRifleStatCache.CooldownSeconds = CooldownSeconds;
	}

	OutDamage = Damage;
	OutRange = Range;
	OutCooldownSeconds = CooldownSeconds;
}

UKodUnitDefinition* UKodSlice0Bootstrap::ResolveUnit(FName DefinitionId, UObject* Outer)
{
	return Cast<UKodUnitDefinition>(ResolveDefinition(DefinitionId, Outer));
}

UKodBuildingDefinition* UKodSlice0Bootstrap::ResolveBuilding(FName DefinitionId, UObject* Outer)
{
	return Cast<UKodBuildingDefinition>(ResolveDefinition(DefinitionId, Outer));
}

UKodFactionDefinition* UKodSlice0Bootstrap::ResolveFaction(FName DefinitionId, UObject* Outer)
{
	return Cast<UKodFactionDefinition>(ResolveDefinition(DefinitionId, Outer));
}

UKodResourceNodeDefinition* UKodSlice0Bootstrap::ResolveResourceNode(FName DefinitionId, UObject* Outer)
{
	return Cast<UKodResourceNodeDefinition>(ResolveDefinition(DefinitionId, Outer));
}

UKodWeaponDefinition* UKodSlice0Bootstrap::MakeRangerRifle(UObject* Outer)
{
	UKodWeaponDefinition* W = NewObject<UKodWeaponDefinition>(Outer, FName(KodWardenPaths::Id_RangerRifle), RF_Public | RF_Transient);
	W->DefinitionId = FName(KodWardenPaths::Id_RangerRifle);
	W->DisplayName = NSLOCTEXT("Kod", "Weapon_RangerRifle", "Ranger Rifle");
	W->Damage = RangerRifleDamage;
	W->Range = RangerRifleRange;
	// Stand-in matches the code fallback (13 ticks). The uasset's CooldownSeconds (shop sheet 0.35)
	// replaces it once /Game/Warden/Data/RangerRifle loads.
	W->CooldownSeconds = RangerRifleCooldownSeconds;
	return W;
}

UKodAbilityDefinition* UKodSlice0Bootstrap::MakeCombatStim(UObject* Outer)
{
	UKodAbilityDefinition* Ability = NewObject<UKodAbilityDefinition>(Outer, FName(KodWardenPaths::Id_CombatStim), RF_Public | RF_Transient);
	Ability->AbilityId = FName(KodWardenPaths::Id_CombatStim);
	Ability->DisplayName = NSLOCTEXT("Kod", "Ability_CombatStim", "Stim");
	Ability->Hotkey = FName(TEXT("T"));
	Ability->Credits = 0;
	Ability->Luminene = 0;
	Ability->HealthCost = 10;
	Ability->CooldownTicks = 0;
	Ability->DurationTicks = 11 * 16;
	Ability->Effect = EKodAbilityEffect::CombatStim;
	Ability->MovePermille = 1500;
	Ability->AttackPermille = 1500;
	return Ability;
}

UKodUnitDefinition* UKodSlice0Bootstrap::MakeMarine(UObject* Outer, UKodWeaponDefinition* Rifle)
{
	UKodUnitDefinition* U = NewObject<UKodUnitDefinition>(Outer, FName(KodWardenPaths::Id_Marine), RF_Public | RF_Transient);
	U->DefinitionId = FName(KodWardenPaths::Id_Marine);
	U->DisplayName = NSLOCTEXT("Kod", "Unit_Marine", "Marine");
	U->UnitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Unit.Marine")), false);
	U->MaxHealth = 120.f;
	U->Armor = 0.f;
	U->bBiological = true;
	U->bInfantry = true;
	U->CargoSize = 1;
	U->MoveSpeed = 450.f;
	U->SightRadius = 1600.f;
	U->Cost.Credits = 75;
	U->Cost.Luminene = 0;
	U->Cost.Supply = 1;
	U->Cost.BuildTime = 80;
	U->BuildTicks = 80; // 5 × 16
	U->BuildTimeSeconds = KodBuildTicks::BuildTicksToSeconds(80);
	if (Rifle)
	{
		U->PrimaryWeapon = Rifle;
	}
	// Presentation stays unset. The current body is SM_Ranger_Body, wired on
	// /Game/Warden/Data/Marine after NAOMI renames the old Ranger DataAsset.
	// Do not assign that path here — the uasset is local, and a miss would log a load error on every spawn.
	U->SkeletalMesh.Reset();
	U->StaticMesh.Reset();
	if (UKodAbilityDefinition* Stim = MakeCombatStim(U))
	{
		U->Abilities.Add(Stim->ToSpec());
	}
	return U;
}

UKodUnitDefinition* UKodSlice0Bootstrap::MakeScinderInfantry(UObject* Outer, UKodWeaponDefinition* Rifle)
{
	UKodUnitDefinition* Unit = NewObject<UKodUnitDefinition>(Outer, FName(KodWardenPaths::Id_ScinderInfantry), RF_Public | RF_Transient);
	Unit->DefinitionId = FName(KodWardenPaths::Id_ScinderInfantry);
	Unit->DisplayName = NSLOCTEXT("Kod", "Unit_ScinderInfantry", "Scinder Infantry");
	Unit->MaxHealth = 120.f;
	Unit->Armor = 0.f;
	Unit->bBiological = true;
	Unit->bInfantry = true;
	Unit->CargoSize = 1;
	Unit->MoveSpeed = 450.f;
	Unit->SightRadius = 1600.f;
	Unit->Cost.Credits = 75;
	Unit->Cost.Luminene = 0;
	Unit->Cost.Supply = 1;
	Unit->Cost.BuildTime = 80;
	Unit->BuildTicks = 80;
	Unit->BuildTimeSeconds = KodBuildTicks::BuildTicksToSeconds(80);
	if (Rifle)
	{
		Unit->PrimaryWeapon = Rifle;
	}
	Unit->SkeletalMesh.Reset();
	Unit->StaticMesh.Reset();
	return Unit;
}

UKodUnitDefinition* UKodSlice0Bootstrap::MakeScnFighter(UObject* Outer)
{
	UKodUnitDefinition* Unit = NewObject<UKodUnitDefinition>(Outer, FName(KodWardenPaths::Id_ScnFighter), RF_Public | RF_Transient);
	Unit->DefinitionId = FName(KodWardenPaths::Id_ScnFighter);
	Unit->DisplayName = NSLOCTEXT("Kod", "Unit_ScnFighter", "Fighter");
	Unit->MaxHealth = 35.f;
	Unit->Armor = 0.f;
	Unit->bBiological = true;
	Unit->bInfantry = true;
	Unit->CargoSize = 1;
	Unit->MoveSpeed = 700.f;
	Unit->SightRadius = 1400.f;
	Unit->Cost.Credits = 40;
	Unit->Cost.Supply = 1;
	Unit->Cost.BuildTime = 14 * 16;
	Unit->BuildTicks = 14 * 16;
	Unit->SkeletalMesh.Reset();
	Unit->StaticMesh.Reset();
	return Unit;
}

UKodUnitDefinition* UKodSlice0Bootstrap::MakeScnTwinblade(UObject* Outer)
{
	UKodUnitDefinition* Unit = NewObject<UKodUnitDefinition>(Outer, FName(KodWardenPaths::Id_ScnTwinblade), RF_Public | RF_Transient);
	Unit->DefinitionId = FName(KodWardenPaths::Id_ScnTwinblade);
	Unit->DisplayName = NSLOCTEXT("Kod", "Unit_ScnTwinblade", "Twin-blade");
	Unit->MaxHealth = 110.f;
	Unit->Armor = 1.f;
	Unit->bBiological = true;
	Unit->bInfantry = true;
	Unit->CargoSize = 1;
	Unit->MoveSpeed = 826.f;
	Unit->SightRadius = 1200.f;
	Unit->Cost.Credits = 75;
	Unit->Cost.Supply = 2;
	Unit->Cost.BuildTime = 22 * 16;
	Unit->BuildTicks = 22 * 16;
	Unit->SkeletalMesh.Reset();
	Unit->StaticMesh.Reset();
	return Unit;
}

UKodUnitDefinition* UKodSlice0Bootstrap::MakeScnWorker(UObject* Outer)
{
	UKodUnitDefinition* Unit = NewObject<UKodUnitDefinition>(Outer, FName(KodWardenPaths::Id_ScnWorker), RF_Public | RF_Transient);
	Unit->DefinitionId = FName(KodWardenPaths::Id_ScnWorker);
	Unit->DisplayName = NSLOCTEXT("Kod", "Unit_ScnWorker", "Worker");
	Unit->MaxHealth = 40.f;
	Unit->Armor = 0.f;
	Unit->bBiological = true;
	Unit->MoveSpeed = 788.f;
	Unit->SightRadius = 1000.f;
	Unit->Cost.Credits = 50;
	Unit->Cost.Supply = 1;
	Unit->Cost.BuildTime = 12 * 16;
	Unit->BuildTicks = 12 * 16;
	Unit->bCanGather = true;
	Unit->SkeletalMesh.Reset();
	Unit->StaticMesh.Reset();
	return Unit;
}

UKodUnitDefinition* UKodSlice0Bootstrap::MakeHideOx(UObject* Outer)
{
	FKodUnitCsvRow Row;
	if (!FKodUnitSheet::Find(FName(KodWardenPaths::Id_ScnHideOx), Row))
	{
		return nullptr;
	}
	UKodUnitDefinition* Unit = NewObject<UKodUnitDefinition>(Outer, Row.Id, RF_Public | RF_Transient);
	Unit->DefinitionId = Row.Id;
	Unit->DisplayName = FText::FromString(Row.DisplayName);
	Unit->MaxHealth = Row.Hp;
	Unit->Armor = static_cast<float>(Row.Armor);
	Unit->bBiological = Row.HasTag(TEXT("Biological")) || Row.HasTag(TEXT("Organic"));
	Unit->bInfantry = Row.HasTag(TEXT("Infantry"));
	Unit->bFlyer = Row.HasTag(TEXT("Air")) || Row.HasTag(TEXT("Flyer"));
	Unit->MoveSpeed = Row.MoveSpeedUU();
	Unit->SightRadius = static_cast<float>(Row.SightUU());
	Unit->Cost.Credits = Row.Credits;
	Unit->Cost.Luminene = Row.Luminene;
	Unit->Cost.Supply = Row.SupplyCost;
	Unit->Cost.BuildTime = Row.BuildTicks();
	Unit->BuildTicks = Row.BuildTicks();
	Unit->BuildTimeSeconds = Row.BuildSeconds;
	Unit->PrimaryWeapon.Reset();
	Unit->SkeletalMesh.Reset();
	Unit->StaticMesh.Reset();
	return Unit;
}

UKodUnitDefinition* UKodSlice0Bootstrap::MakeDozer(UObject* Outer)
{
	UKodUnitDefinition* U = NewObject<UKodUnitDefinition>(Outer, FName(KodWardenPaths::Id_Dozer), RF_Public | RF_Transient);
	U->DefinitionId = FName(KodWardenPaths::Id_Dozer);
	U->DisplayName = NSLOCTEXT("Kod", "Unit_Dozer", "AMP");
	U->UnitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Unit.Dozer")), false);
	U->MaxHealth = 200.f;
	U->Armor = 1.f;
	U->MoveSpeed = 350.f;
	U->SightRadius = 1200.f;
	U->Cost.Credits = KodEconomyDefaults::AmpCredits;
	U->Cost.Luminene = KodEconomyDefaults::AmpLuminene;
	U->Cost.Supply = KodEconomyDefaults::AmpSupply;
	U->Cost.BuildTime = KodEconomyDefaults::AmpBuildTicks;
	U->BuildTicks = KodEconomyDefaults::AmpBuildTicks;
	U->BuildTimeSeconds = static_cast<float>(KodEconomyDefaults::AmpBuildSeconds);
	U->bCanGather = true;
	U->bBiological = true;
	U->CargoSize = 0;
	U->PrimaryWeapon.Reset();
	// Shipped default is the cube. The truck prototype is KodWardenPaths::ProtoAmpSkeletalMesh
	// and is assigned on the editable Dozer DataAsset, not here.
	U->SkeletalMesh.Reset();
	U->StaticMesh.Reset();
	return U;
}

UKodBuildingDefinition* UKodSlice0Bootstrap::MakeCommandCenter(UObject* Outer, UKodUnitDefinition* Dozer)
{
	UKodBuildingDefinition* B = NewObject<UKodBuildingDefinition>(Outer, FName(KodWardenPaths::Id_CommandCenter), RF_Public | RF_Transient);
	B->DefinitionId = FName(KodWardenPaths::Id_CommandCenter);
	B->DisplayName = NSLOCTEXT("Kod", "Building_CommandCenter", "Command Center");
	B->BuildingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Building.CommandCenter")), false);
	B->MaxHealth = 2500.f;
	B->Cost.Credits = 400;
	B->Cost.Luminene = 0;
	B->Cost.Supply = 0;
	B->Cost.BuildTime = 71 * 16;
	B->BuildTicks = B->Cost.BuildTime;
	B->BuildTimeSeconds = 71.f;
	B->SupplyProvided = KodEconomyDefaults::SupplyPerHq;
	B->bIsDropOff = true;
	B->bTownHall = true;
	B->MenuTier = 0;
	B->FootprintX = 5;
	B->FootprintY = 5;
	B->Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(KodWardenPaths::ProtoHqMesh));
	B->PowerProvided = 10.f;
	B->PowerConsumed = 0.f;
	B->SightRadius = 2000.f;
	B->TrainableUnits.Reset();
	if (Dozer)
	{
		B->TrainableUnits.Add(Dozer);
	}
	return B;
}

UKodBuildingDefinition* UKodSlice0Bootstrap::MakeBarracks(UObject* Outer, UKodUnitDefinition* Marine)
{
	UKodBuildingDefinition* B = NewObject<UKodBuildingDefinition>(Outer, FName(KodWardenPaths::Id_Barracks), RF_Public | RF_Transient);
	B->DefinitionId = FName(KodWardenPaths::Id_Barracks);
	B->DisplayName = NSLOCTEXT("Kod", "Building_Barracks", "Barracks");
	B->BuildingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Building.Barracks")), false);
	B->MaxHealth = 1500.f;
	B->Cost.Credits = 150;
	B->Cost.Luminene = 0;
	B->Cost.Supply = 0;
	B->Cost.BuildTime = 46 * 16;
	B->BuildTicks = B->Cost.BuildTime;
	B->BuildTimeSeconds = 46.f;
	B->FootprintX = 3;
	B->FootprintY = 3;
	B->RequiresId = FName(KodWardenPaths::Id_SupplyDepot);
	B->MenuTier = 0;
	B->Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(KodWardenPaths::ProtoBarracksMesh));
	B->PowerConsumed = 2.f;
	B->PowerProvided = 0.f;
	B->SightRadius = 1400.f;
	B->TrainableUnits.Reset();
	if (Marine)
	{
		B->TrainableUnits.Add(Marine);
	}
	return B;
}

UKodBuildingDefinition* UKodSlice0Bootstrap::MakeSupplyDepot(UObject* Outer)
{
	UKodBuildingDefinition* B = NewObject<UKodBuildingDefinition>(Outer, FName(KodWardenPaths::Id_SupplyDepot), RF_Public | RF_Transient);
	B->DefinitionId = FName(KodWardenPaths::Id_SupplyDepot);
	B->DisplayName = NSLOCTEXT("Kod", "Building_SupplyDepot", "Supply Depot");
	B->BuildingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Building.SupplyDepot")), false);
	B->MaxHealth = 400.f;
	B->Cost.Credits = 100;
	B->Cost.Luminene = 0;
	B->Cost.Supply = 0;
	B->Cost.BuildTime = 21 * 16;
	B->BuildTicks = B->Cost.BuildTime;
	B->BuildTimeSeconds = 21.f;
	B->SupplyProvided = KodEconomyDefaults::SupplyPerDepot;
	B->bIsDropOff = false;
	B->bCanLower = true;
	B->MenuTier = 0;
	B->FootprintX = 2;
	B->FootprintY = 2;
	B->Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(KodWardenPaths::ProtoDepotMesh));
	return B;
}

UKodBuildingDefinition* UKodSlice0Bootstrap::MakeRig(UObject* Outer)
{
	UKodBuildingDefinition* B = NewObject<UKodBuildingDefinition>(Outer, FName(KodWardenPaths::Id_Rig), RF_Public | RF_Transient);
	B->DefinitionId = FName(KodWardenPaths::Id_Rig);
	B->DisplayName = NSLOCTEXT("Kod", "Building_Rig", "Rig");
	B->BuildingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Building.Rig")), false);
	B->MaxHealth = 500.f;
	B->Cost.Credits = 75;
	B->Cost.Luminene = 0;
	B->Cost.Supply = 0;
	B->Cost.BuildTime = 30 * 16;
	B->BuildTicks = B->Cost.BuildTime;
	B->BuildTimeSeconds = 30.f;
	B->FootprintX = 3;
	B->FootprintY = 3;
	B->bExtractor = true;
	B->SupplyProvided = 0;
	B->bIsDropOff = false;
	B->MenuTier = 0;
	return B;
}

UKodUnitDefinition* UKodSlice0Bootstrap::MakeMedic(UObject* Outer)
{
	UKodUnitDefinition* U = NewObject<UKodUnitDefinition>(Outer, FName(KodWardenPaths::Id_Medic), RF_Public | RF_Transient);
	U->DefinitionId = FName(KodWardenPaths::Id_Medic);
	U->DisplayName = NSLOCTEXT("Kod", "Unit_Medic", "Medic");
	U->UnitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Unit.Medic")), false);
	U->MaxHealth = 60.f;
	U->Armor = 1.f;
	U->MoveSpeed = 400.f;
	U->SightRadius = 1400.f;
	U->Cost.Credits = 50;
	U->Cost.Luminene = 25;
	U->Cost.Supply = 1;
	U->Cost.BuildTime = 20 * 16;
	U->BuildTicks = U->Cost.BuildTime;
	U->BuildTimeSeconds = 20.f;
	U->bBiological = true;
	U->bInfantry = true;
	U->bHealer = true;
	U->CargoSize = 1;
	U->EnergyMax = 200;
	U->EnergyStart = 50;
	U->RequiresId = FName(KodWardenPaths::Id_EngineeringBay);
	return U;
}

UKodBuildingDefinition* UKodSlice0Bootstrap::MakeEngineeringBay(UObject* Outer)
{
	UKodBuildingDefinition* B = NewObject<UKodBuildingDefinition>(Outer, FName(KodWardenPaths::Id_EngineeringBay), RF_Public | RF_Transient);
	B->DefinitionId = FName(KodWardenPaths::Id_EngineeringBay);
	B->DisplayName = NSLOCTEXT("Kod", "Building_EngineeringBay", "Engineering Bay");
	B->MaxHealth = 850.f;
	B->Cost.Credits = 125;
	B->Cost.Luminene = 0;
	B->Cost.BuildTime = 25 * 16;
	B->BuildTicks = B->Cost.BuildTime;
	B->BuildTimeSeconds = 25.f;
	B->FootprintX = 3;
	B->FootprintY = 3;
	B->MenuTier = 0;
	B->SightRadius = 1400.f;
	return B;
}

UKodBuildingDefinition* UKodSlice0Bootstrap::MakeBunker(UObject* Outer)
{
	UKodBuildingDefinition* B = NewObject<UKodBuildingDefinition>(Outer, FName(KodWardenPaths::Id_Bunker), RF_Public | RF_Transient);
	B->DefinitionId = FName(KodWardenPaths::Id_Bunker);
	B->DisplayName = NSLOCTEXT("Kod", "Building_Bunker", "Vanguard Bunker");
	B->BuildingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Building.VG_BUNKER")), false);
	B->AttributeTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Attribute.Armored")), false));
	B->AttributeTags.AddTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Attribute.Structure")), false));
	// Placeholder HP, armour, and cost until the Director stamps the balance row.
	B->MaxHealth = 400.f;
	B->Armor = 1;
	B->Cost.Credits = 100;
	B->Cost.Luminene = 0;
	B->Cost.BuildTime = 29 * 16;
	B->BuildTicks = B->Cost.BuildTime;
	B->BuildTimeSeconds = 29.f;
	B->FootprintX = 3;
	B->FootprintY = 3;
	B->MenuTier = 0;
	B->CargoCap = 4;
	B->RequiresId = FName(KodWardenPaths::Id_Barracks);
	B->SightRadius = 1400.f;
	return B;
}

UKodBuildingDefinition* UKodSlice0Bootstrap::MakeTurret(UObject* Outer)
{
	UKodBuildingDefinition* B = NewObject<UKodBuildingDefinition>(Outer, FName(KodWardenPaths::Id_Turret), RF_Public | RF_Transient);
	B->DefinitionId = FName(KodWardenPaths::Id_Turret);
	B->DisplayName = NSLOCTEXT("Kod", "Building_Turret", "Turret");
	B->MaxHealth = 250.f;
	B->Cost.Credits = 100;
	B->Cost.Luminene = 0;
	B->Cost.BuildTime = 25 * 16;
	B->BuildTicks = B->Cost.BuildTime;
	B->BuildTimeSeconds = 25.f;
	B->FootprintX = 2;
	B->FootprintY = 2;
	B->MenuTier = 1;
	B->RequiresId = FName(KodWardenPaths::Id_EngineeringBay);
	B->AttackDamage = 12;
	B->AttackRangeUU = 7 * 200;
	B->bAttacksAir = true;
	B->SightRadius = 1400.f;
	return B;
}

UKodFactionDefinition* UKodSlice0Bootstrap::MakeUSA(UObject* Outer, UKodBuildingDefinition* CC, UKodUnitDefinition* Dozer, UKodUnitDefinition* Marine)
{
	UKodFactionDefinition* F = NewObject<UKodFactionDefinition>(Outer, FName(KodWardenPaths::Id_USA), RF_Public | RF_Transient);
	F->DefinitionId = FName(KodWardenPaths::Id_USA);
	F->DisplayName = NSLOCTEXT("Kod", "Faction_USA", "USA");
	if (CC)
	{
		F->StartingBuilding = CC;
	}
	F->StartingUnits.Reset();
	if (Dozer)
	{
		F->StartingUnits.Add(Dozer);
	}
	if (Marine)
	{
		F->StartingUnits.Add(Marine);
		F->StartingUnits.Add(Marine);
	}
	return F;
}

UKodResourceNodeDefinition* UKodSlice0Bootstrap::MakeJadeiteNode(UObject* Outer)
{
	UKodResourceNodeDefinition* Node = NewObject<UKodResourceNodeDefinition>(Outer, FName(KodWardenPaths::Id_JadeiteNode), RF_Public | RF_Transient);
	Node->DefinitionId = FName(KodWardenPaths::Id_JadeiteNode);
	Node->DisplayName = NSLOCTEXT("Kod", "Resource_JadeiteNode", "Jadeite");
	Node->NodeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Resource.Jadeite")), false);
	Node->ResourceType = EKodResourceType::Jadeite;
	Node->Amount = KodEconomyDefaults::JadeiteNodeAmount;
	Node->HarvestPerTrip = KodEconomyDefaults::JadeiteHarvestPerTrip;
	Node->HarvestTicks = KodEconomyDefaults::JadeiteHarvestTicks;
	Node->TricklePerTrip = 0;
	Node->GatherStandUU = KodEconomyDefaults::JadeiteGatherStandUU;
	Node->Mesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(KodWardenPaths::ProtoJadeiteMesh));
	return Node;
}

UKodResourceNodeDefinition* UKodSlice0Bootstrap::MakeLumineneVent(UObject* Outer)
{
	UKodResourceNodeDefinition* Node = NewObject<UKodResourceNodeDefinition>(Outer, FName(KodWardenPaths::Id_LumineneVent), RF_Public | RF_Transient);
	Node->DefinitionId = FName(KodWardenPaths::Id_LumineneVent);
	Node->DisplayName = NSLOCTEXT("Kod", "Resource_LumineneVent", "Luminene");
	Node->NodeTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Resource.Luminene")), false);
	Node->ResourceType = EKodResourceType::Luminene;
	Node->Amount = KodEconomyDefaults::LumineneNodeAmount;
	Node->HarvestPerTrip = KodEconomyDefaults::LumineneHarvestPerTrip;
	Node->HarvestTicks = KodEconomyDefaults::LumineneHarvestTicks;
	Node->TricklePerTrip = KodEconomyDefaults::LumineneTricklePerTrip;
	Node->GatherStandUU = KodEconomyDefaults::LumineneGatherStandUU;
	Node->Mesh.Reset();
	return Node;
}

AKodUnit* UKodSlice0Bootstrap::SpawnRanger(UWorld* World, const FTransform& Transform, int32 TeamId, bool bForceCubeBody)
{
	return SpawnMarine(World, Transform, TeamId, bForceCubeBody);
}

AKodUnit* UKodSlice0Bootstrap::SpawnMarine(UWorld* World, const FTransform& Transform, int32 TeamId, bool bForceCubeBody)
{
	const FVector Location = Transform.GetLocation();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnMarine failed: no world (requested %s)"), *Location.ToString());
		return nullptr;
	}
	UKodUnitDefinition* Def = ResolveUnit(FName(KodWardenPaths::Id_Marine), GetTransientPackage());
	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnMarine failed: Marine definition missing at %s"), *Location.ToString());
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AKodUnit* Unit = World->SpawnActor<AKodUnit>(AKodUnit::StaticClass(), Transform, Params);
	if (!Unit)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnMarine failed: SpawnActor returned null at %s"), *Location.ToString());
		return nullptr;
	}
	Unit->Definition = Def;
	Unit->TeamId = TeamId;
	// Before ApplyDefinition so ApplyBodyMesh does not mount SM_Ranger_Body.
	if (bForceCubeBody)
	{
		Unit->SetForceCubeBody(true);
	}
	Unit->ApplyDefinition(Def);
	if (bForceCubeBody)
	{
		Unit->ApplyHostileCubeTint();
	}
	UE_LOG(LogTemp, Warning, TEXT("SpawnMarine spawned %s at %s"), *Unit->GetName(), *Unit->GetActorLocation().ToString());
	return Unit;
}

AKodUnit* UKodSlice0Bootstrap::SpawnDozer(UWorld* World, const FTransform& Transform, int32 TeamId)
{
	return SpawnGatherer(World, Transform, TeamId, FName(KodWardenPaths::Id_Dozer));
}

AKodUnit* UKodSlice0Bootstrap::SpawnGatherer(UWorld* World, const FTransform& Transform, int32 TeamId, FName DefinitionId)
{
	const FVector Location = Transform.GetLocation();
	if (DefinitionId.IsNone())
	{
		DefinitionId = FName(KodWardenPaths::Id_Dozer);
	}
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnGatherer failed: no world (requested %s)"), *Location.ToString());
		return nullptr;
	}
	UKodUnitDefinition* Def = ResolveUnit(DefinitionId, GetTransientPackage());
	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnGatherer failed: %s definition missing at %s"), *DefinitionId.ToString(), *Location.ToString());
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AKodUnit* Unit = World->SpawnActor<AKodUnit>(AKodUnit::StaticClass(), Transform, Params);
	if (!Unit)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnGatherer failed: SpawnActor returned null at %s"), *Location.ToString());
		return nullptr;
	}
	Unit->Definition = Def;
	Unit->TeamId = TeamId;
	Unit->Tags.AddUnique(FName(TEXT("KodUnitDozer")));
	Unit->ApplyDefinition(Def);
	UE_LOG(LogTemp, Log, TEXT("SpawnGatherer Id=%s spawned %s at %s"), *DefinitionId.ToString(), *Unit->GetName(), *Unit->GetActorLocation().ToString());
	return Unit;
}
