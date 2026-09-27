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

	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static UKodUnitDefinition* ResolveUnit(FName DefinitionId, UObject* Outer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static UKodBuildingDefinition* ResolveBuilding(FName DefinitionId, UObject* Outer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static UKodFactionDefinition* ResolveFaction(FName DefinitionId, UObject* Outer = nullptr);

	/** Spawn one Ranger at Transform; MaxHealth from DA only. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Slice0")
	static class AKodUnit* SpawnRanger(UWorld* World, const FTransform& Transform, int32 TeamId = 0);

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
