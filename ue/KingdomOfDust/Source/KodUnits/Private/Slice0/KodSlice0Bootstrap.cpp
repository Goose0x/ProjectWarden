#include "Slice0/KodSlice0Bootstrap.h"
#include "GameplayTagContainer.h"
#include "Slice0/KodDataCatalog.h"
#include "Data/KodWeaponDefinition.h"
#include "Data/KodUnitDefinition.h"
#include "Data/KodBuildingDefinition.h"
#include "Data/KodFactionDefinition.h"
#include "Data/KodResourceNodeDefinition.h"
#include "Actors/KodUnit.h"
#include "Warden/KodWardenPaths.h"
#include "Sim/KodBuildTicks.h"
#include "Engine/World.h"
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
	UKodResourceNodeDefinition* JadeiteNode = MakeJadeiteNode(Catalog);
	UKodResourceNodeDefinition* LumineneVent = MakeLumineneVent(Catalog);

	Catalog->RegisterAsset(FName(KodWardenPaths::Id_RangerRifle), Rifle);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Ranger), Ranger);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Dozer), Dozer);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_CommandCenter), CC);
	Catalog->RegisterAsset(FName(KodWardenPaths::Id_Barracks), Barracks);
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

UKodUnitDefinition* UKodSlice0Bootstrap::MakeRanger(UObject* Outer, UKodWeaponDefinition* Rifle)
{
	UKodUnitDefinition* U = NewObject<UKodUnitDefinition>(Outer, FName(KodWardenPaths::Id_Ranger), RF_Public | RF_Transient);
	U->DefinitionId = FName(KodWardenPaths::Id_Ranger);
	// Player-facing name. DefinitionId, asset path, and logs stay Ranger.
	U->DisplayName = NSLOCTEXT("Kod", "Unit_Ranger", "Marine");
	U->UnitTag = FGameplayTag::RequestGameplayTag(FName(TEXT("Kod.Unit.Ranger")), false);
	U->MaxHealth = 120.f;
	U->Armor = 0.f;
	U->MoveSpeed = 450.f;
	U->SightRadius = 1600.f;
	U->BuildCostCash = 75;
	U->BuildTicks = 80; // 5 × 16
	U->BuildTimeSeconds = KodBuildTicks::BuildTicksToSeconds(80);
	if (Rifle)
	{
		U->PrimaryWeapon = Rifle;
	}
	// Presentation stays unset. Soft Dev wires StaticMesh to KodWardenPaths::RangerBodyStaticMesh
	// on /Game/Warden/Data/Ranger after the local SM_Ranger_Body import. Do not assign that path
	// here — the uasset is local, and a miss would log a load error on every spawn.
	U->SkeletalMesh.Reset();
	U->StaticMesh.Reset();
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
	U->BuildCostCash = 100;
	U->BuildTicks = 128; // 8 × 16
	U->BuildTimeSeconds = KodBuildTicks::BuildTicksToSeconds(128);
	U->bCanGather = true;
	U->PrimaryWeapon.Reset();
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
	B->BuildCostCash = 0;
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
	B->BuildCostCash = 250;
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
	Node->Mesh.Reset();
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
	const FVector Location = Transform.GetLocation();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnRanger failed: no world (requested %s)"), *Location.ToString());
		return nullptr;
	}
	UKodUnitDefinition* Def = ResolveUnit(FName(KodWardenPaths::Id_Ranger), GetTransientPackage());
	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnRanger failed: Ranger definition missing at %s"), *Location.ToString());
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AKodUnit* Unit = World->SpawnActor<AKodUnit>(AKodUnit::StaticClass(), Transform, Params);
	if (!Unit)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnRanger failed: SpawnActor returned null at %s"), *Location.ToString());
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
	UE_LOG(LogTemp, Warning, TEXT("SpawnRanger spawned %s at %s"), *Unit->GetName(), *Unit->GetActorLocation().ToString());
	return Unit;
}

AKodUnit* UKodSlice0Bootstrap::SpawnDozer(UWorld* World, const FTransform& Transform, int32 TeamId)
{
	const FVector Location = Transform.GetLocation();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnDozer failed: no world (requested %s)"), *Location.ToString());
		return nullptr;
	}
	UKodUnitDefinition* Def = ResolveUnit(FName(KodWardenPaths::Id_Dozer), GetTransientPackage());
	if (!Def)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnDozer failed: Dozer definition missing at %s"), *Location.ToString());
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AKodUnit* Unit = World->SpawnActor<AKodUnit>(AKodUnit::StaticClass(), Transform, Params);
	if (!Unit)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnDozer failed: SpawnActor returned null at %s"), *Location.ToString());
		return nullptr;
	}
	Unit->Definition = Def;
	Unit->TeamId = TeamId;
	Unit->Tags.AddUnique(FName(TEXT("KodUnitDozer")));
	Unit->ApplyDefinition(Def);
	UE_LOG(LogTemp, Log, TEXT("SpawnDozer spawned %s at %s"), *Unit->GetName(), *Unit->GetActorLocation().ToString());
	return Unit;
}
