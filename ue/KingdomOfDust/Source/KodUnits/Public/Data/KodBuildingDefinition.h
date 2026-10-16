#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"
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

	/** Combat attributes such as Armored and Structure. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FGameplayTagContainer AttributeTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Combat")
	int32 Armor = 0;

	/** Silhouette for a selected structure. 128 source, scaled by the HUD. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|UI")
	TSoftObjectPtr<UTexture2D> Wireframe;

	/** Command card and queue icon. 128 source, scaled to the slot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|UI")
	TSoftObjectPtr<UTexture2D> CommandIcon;

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

	/** Completed building required before placement. Barracks requires a Depot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	FName RequiresId;

	/** 0 basic (B), 1 advanced (V). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	int32 MenuTier = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	bool bTownHall = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	bool bCanLower = false;

	/** Bunker cargo. 0 means the building holds no units. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	int32 CargoCap = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Combat")
	int32 AttackDamage = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Combat")
	int32 AttackRangeUU = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Combat")
	bool bAttacksAir = false;

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

	/** Low-HP smoke. Left empty. The building spawns it only when this loads. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|FX")
	TSoftObjectPtr<UNiagaraSystem> LowHealthSmoke;

	/** Low-HP fire. Left empty. The building spawns it only when this loads. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|FX")
	TSoftObjectPtr<UNiagaraSystem> LowHealthFire;

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
