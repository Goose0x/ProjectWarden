#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Sim/KodBuildTicks.h"
#include "Sim/KodResourceTypes.h"
#include "KodBuildingDefinition.generated.h"

class UKodUnitDefinition;

/**
 * Primary data asset for a building archetype.
 * Runtime MUST load MaxHealth / BuildTicks from this DA.
 * Slice 0 id law: CommandCenter, Barracks under /Game/Warden/Data/.
 */
UCLASS(BlueprintType)
class KODUNITS_API UKodBuildingDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Must match asset name exactly (e.g. Barracks). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FName DefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FGameplayTag BuildingTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Combat")
	float MaxHealth = 1000.f;

	/** Credits are Jadeite. BuildTime is sim ticks. Replaces BuildCostCash. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	FKodTrainCost Cost;

	/** HQ 15, Supply Depot 8. Added to the team cap while the structure is registered. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 SupplyProvided = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	bool bIsDropOff = false;

	/** Integer build-grid cells. CC 5, Depot 2, Barracks 3, Rig 3. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 FootprintX = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 FootprintY = 1;

	/** Valid only when the footprint covers an open Luminene vent. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	bool bExtractor = false;

	/** Optional presentation. Empty keeps the placed greybox mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	TSoftObjectPtr<UStaticMesh> Mesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	FVector MeshScale = FVector(1.f, 1.f, 1.f);

	/** Authoritative build duration in sim ticks. Barracks=320 (20×16). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 BuildTicks = 0;

	/** Editor convenience; synced with BuildTicks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	float BuildTimeSeconds = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Production")
	TArray<TSoftObjectPtr<UKodUnitDefinition>> TrainableUnits;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Power")
	float PowerConsumed = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Power")
	float PowerProvided = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Vision")
	float SightRadius = 1200.f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		const FName Id = !DefinitionId.IsNone() ? DefinitionId : GetFName();
		return FPrimaryAssetId(TEXT("KodBuildingDefinition"), Id);
	}

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	int32 GetResolvedBuildTicks() const
	{
		if (Cost.BuildTime > 0)
		{
			return Cost.BuildTime;
		}
		return KodBuildTicks::ResolveBuildTicks(BuildTicks, BuildTimeSeconds);
	}

	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void SyncBuildTimingFromSeconds()
	{
		BuildTicks = KodBuildTicks::SecondsToBuildTicks(BuildTimeSeconds);
	}

	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void SyncBuildTimingFromTicks()
	{
		BuildTimeSeconds = KodBuildTicks::BuildTicksToSeconds(BuildTicks);
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
