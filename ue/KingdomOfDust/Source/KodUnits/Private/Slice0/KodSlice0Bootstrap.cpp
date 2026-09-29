#include "Slice0/KodSlice0Bootstrap.h"
#include "GameplayTagContainer.h"
#include "Slice0/KodDataCatalog.h"
#include "Data/KodWeaponDefinition.h"
#include "Data/KodUnitDefinition.h"
#include "Data/KodBuildingDefinition.h"
#include "Data/KodFactionDefinition.h"
#include "Actors/KodUnit.h"
#include "Warden/KodWardenPaths.h"
#include "Sim/KodBuildTicks.h"
#include "Engine/World.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"

TWeakObjectPtr<UKodDataCatalog> UKodSlice0Bootstrap::GCatalog;

static UObject* TryLoadWardenAsset(FName DefinitionId)
{
	const FSoftObjectPath Path = KodWardenPaths::MakeSoftPath(*DefinitionId.ToString());
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
	UKodUnitDefinition* Ranger = MakeRanger(Catalog, Rifle);
	UKodUnitDefinition* Dozer = MakeDozer(Catalog);
	UKodBuildingDefinition* CC = MakeCommandCenter(Catalog, Dozer);
	UKodBuildingDefinition* Barracks = MakeBarracks(Catalog, Ranger);
	UKodFactionDefinition* USA = MakeUSA(Catalog, CC, Dozer, Ranger);

	Catalog->RegisterAsset(FName(KodWardenPaths::Id_RangerRifle), Rifle);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Ranger), Ranger);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Dozer), Dozer);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_CommandCenter), CC);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Barracks), Barracks);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_USA), USA);

	GCatalog = Catalog;
	return Catalog;
}

UObject* UKodSlice0Bootstrap::ResolveDefinition(FName DefinitionId, UObject* Outer)
{
	if (DefinitionId.IsNone())
	{
		return nullptr;
	}
	if (UObject* Loaded = TryLoadWardenAsset(DefinitionId))
	{
		return Loaded;
	}
	UKodDataCatalog* Catalog = EnsureCatalog(Outer);
	return Catalog ? Catalog->FindAsset(DefinitionId) : nullptr;
}

UKodWeaponDefinition* UKodSlice0Bootstrap::ResolveWeapon(FName DefinitionId, UObject* Outer)
{
	return Cast<UKodWeaponDefinition>(ResolveDefinition(DefinitionId, Outer));
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

UKodWeaponDefinition* UKodSlice0Bootstrap::MakeRangerRifle(UObject* Outer)
{
	UKodWeaponDefinition* W = NewObject<UKodWeaponDefinition>(Outer, FName(KodWardenPaths::Id_RangerRifle), RF_Public | RF_Transient);
	W->DefinitionId = FName(KodWardenPaths::Id_RangerRifle);
	W->DisplayName = NSLOCTEXT("Kod", "Weapon_RangerRifle", "Ranger Rifle");
	W->Damage = 12.f;
	W->Range = 900.f;
	W->CooldownSeconds = 0.35f;
	return W;
}

UKodUnitDefinition* UKodSlice0Bootstrap::MakeRanger(UObject* Outer, UKodWeaponDefinition* Rifle)
{
	UKodUnitDefinition* U = NewObject<UKodUnitDefinition>(Outer, FName(KodWardenPaths::Id_Ranger), RF_Public | RF_Transient);
	U->DefinitionId = FName(KodWardenPaths::Id_Ranger);
	U->DisplayName = NSLOCTEXT("Kod", "Unit_Ranger", "Ranger");
	U->UnitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Unit.Ranger")), false);
	U->MaxHealth = 120.f;
	U->Armor = 0.f;
	U->MoveSpeed = 450.f;
	U->SightRadius = 1600.f;
	U->BuildCostDust = 75;
	U->BuildTicks = 80; // 5 × 16
	U->BuildTimeSeconds = KodBuildTicks::BuildTicksToSeconds(80);
	if (Rifle)
	{
		U->PrimaryWeapon = Rifle;
	}
	return U;
}

UKodUnitDefinition* UKodSlice0Bootstrap::MakeDozer(UObject* Outer)
{
	UKodUnitDefinition* U = NewObject<UKodUnitDefinition>(Outer, FName(KodWardenPaths::Id_Dozer), RF_Public | RF_Transient);
	U->DefinitionId = FName(KodWardenPaths::Id_Dozer);
	U->DisplayName = NSLOCTEXT("Kod", "Unit_Dozer", "Dozer");
	U->UnitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Unit.Dozer")), false);
	U->MaxHealth = 200.f;
	U->Armor = 1.f;
	U->MoveSpeed = 350.f;
	U->SightRadius = 1200.f;
	U->BuildCostDust = 100;
	U->BuildTicks = 128; // 8 × 16
	U->BuildTimeSeconds = KodBuildTicks::BuildTicksToSeconds(128);
	U->PrimaryWeapon.Reset();
	return U;
}

UKodBuildingDefinition* UKodSlice0Bootstrap::MakeCommandCenter(UObject* Outer, UKodUnitDefinition* Dozer)
{
	UKodBuildingDefinition* B = NewObject<UKodBuildingDefinition>(Outer, FName(KodWardenPaths::Id_CommandCenter), RF_Public | RF_Transient);
	B->DefinitionId = FName(KodWardenPaths::Id_CommandCenter);
	B->DisplayName = NSLOCTEXT("Kod", "Building_CommandCenter", "Command Center");
	B->BuildingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Building.CommandCenter")), false);
	B->MaxHealth = 2500.f;
	B->BuildCostDust = 0;
	B->BuildTicks = 0;
	B->BuildTimeSeconds = 0.f;
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

UKodBuildingDefinition* UKodSlice0Bootstrap::MakeBarracks(UObject* Outer, UKodUnitDefinition* Ranger)
{
	UKodBuildingDefinition* B = NewObject<UKodBuildingDefinition>(Outer, FName(KodWardenPaths::Id_Barracks), RF_Public | RF_Transient);
	B->DefinitionId = FName(KodWardenPaths::Id_Barracks);
	B->DisplayName = NSLOCTEXT("Kod", "Building_Barracks", "Barracks");
	B->BuildingTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Building.Barracks")), false);
	B->MaxHealth = 1500.f;
	B->BuildCostDust = 250;
	B->BuildTicks = 320; // 20 × 16
	B->BuildTimeSeconds = KodBuildTicks::BuildTicksToSeconds(320);
	B->PowerConsumed = 2.f;
	B->PowerProvided = 0.f;
	B->SightRadius = 1400.f;
	B->TrainableUnits.Reset();
	if (Ranger)
	{
		B->TrainableUnits.Add(Ranger);
	}
	return B;
}

UKodFactionDefinition* UKodSlice0Bootstrap::MakeUSA(UObject* Outer, UKodBuildingDefinition* CC, UKodUnitDefinition* Dozer, UKodUnitDefinition* Ranger)
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
	if (Ranger)
	{
		F->StartingUnits.Add(Ranger);
		F->StartingUnits.Add(Ranger);
	}
	return F;
}

AKodUnit* UKodSlice0Bootstrap::SpawnRanger(UWorld* World, const FTransform& Transform, int32 TeamId)
{
	if (!World)
	{
		return nullptr;
	}
	UKodUnitDefinition* Def = ResolveUnit(FName(KodWardenPaths::Id_Ranger), GetTransientPackage());
	if (!Def)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AKodUnit* Unit = World->SpawnActor<AKodUnit>(AKodUnit::StaticClass(), Transform, Params);
	if (!Unit)
	{
		return nullptr;
	}
	Unit->Definition = Def;
	Unit->TeamId = TeamId;
	Unit->ApplyDefinition(Def);
	return Unit;
}
