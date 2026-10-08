#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "KodSlice0Bootstrap.generated.h"

class UKodDataCatalog;
class UKodWeaponDefinition;
class UKodUnitDefinition;
class UKodBuildingDefinition;
class UKodFactionDefinition;
class UWorld;

/**
 * Runtime authoring bridge until cooked .uassets exist under /Game/Warden/Data/.
 * NewObject with exact FNames: RangerRifle, Ranger, Dozer, CommandCenter, Barracks, USA.
 * PIE: prefer SoftObjectPath load; on failure use catalog entries from EnsureCatalog.
 */
UCLASS()
class KODUNITS_API UKodSlice0Bootstrap : public UObject
{
	GENERATED_BODY()

public:
	/** Build or return the process-wide Slice 0 catalog (idempotent). */
	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static UKodDataCatalog* EnsureCatalog(UObject* Outer = nullptr);

	/** Resolve by bare id: try /Game/Warden/Data/<Id> then catalog bootstrap. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static UObject* ResolveDefinition(FName DefinitionId, UObject* Outer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static UKodWeaponDefinition* ResolveWeapon(FName DefinitionId, UObject* Outer = nullptr);

	/**
	 * Combat numbers for RangerRifle.
	 * Loads /Game/Warden/Data/RangerRifle when that asset exists.
	 * On a miss, fills the code default (Damage 12, Range 900, 13 sim ticks = 0.8125s)
	 * and logs one warning for this world:
	 * KodWeapon fallback RangerRifle Damage=12 Range=900 Cooldown=0.8125
	 * LogWorld scopes that warning to the current PIE world. Null logs every call.
	 */
	static void ResolveRangerRifleCombatStats(float& OutDamage, float& OutRange, float& OutCooldownSeconds, UWorld* LogWorld = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static UKodUnitDefinition* ResolveUnit(FName DefinitionId, UObject* Outer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static UKodBuildingDefinition* ResolveBuilding(FName DefinitionId, UObject* Outer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static UKodFactionDefinition* ResolveFaction(FName DefinitionId, UObject* Outer = nullptr);

	/**
	 * Spawn one Ranger at Transform. MaxHealth from the DA only.
	 * bForceCubeBody keeps the Engine cube and applies the red hostile tint.
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static class AKodUnit* SpawnRanger(UWorld* World, const FTransform& Transform, int32 TeamId = 0, bool bForceCubeBody = false);

protected:
	static UKodDataCatalog* BuildCatalog(UObject* Outer);
	static UKodWeaponDefinition* MakeRangerRifle(UObject* Outer);
	static UKodUnitDefinition* MakeRanger(UObject* Outer, UKodWeaponDefinition* Rifle);
	static UKodUnitDefinition* MakeDozer(UObject* Outer);
	static UKodBuildingDefinition* MakeCommandCenter(UObject* Outer, UKodUnitDefinition* Dozer);
	static UKodBuildingDefinition* MakeBarracks(UObject* Outer, UKodUnitDefinition* Ranger);
	static UKodFactionDefinition* MakeUSA(UObject* Outer, UKodBuildingDefinition* CC, UKodUnitDefinition* Dozer, UKodUnitDefinition* Ranger);

	static TWeakObjectPtr<UKodDataCatalog> GCatalog;
};
