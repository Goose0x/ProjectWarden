#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "GameplayTagContainer.h"
#include "Sim/KodResourceTypes.h"
#include "KodResourceNodeDefinition.generated.h"

/**
 * Primary data asset for one resource-node archetype.
 * Slice 0 ids: JadeiteNode, LumineneVent under /Game/Warden/Data/.
 * Bootstrap NewObject fills these when the uasset is missing. PIE does not need the file.
 * Mesh empty keeps the engine-shape placeholder on AKodResourceNode.
 * Amount, trip size, harvest ticks, trickle, and stand distance are the sim numbers.
 */
UCLASS(BlueprintType)
class KODUNITS_API UKodResourceNodeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Must match the asset name exactly (JadeiteNode, LumineneVent). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FName DefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FGameplayTag NodeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	EKodResourceType ResourceType = EKodResourceType::Jadeite;

	/** Starting pile. JadeiteNode 1500, LumineneVent 2250. Integer, sim-owned after spawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 Amount = 1500;

	/** One worker trip while the pile remains. Jadeite 5, Luminene 4. Debug harvest can pass a different amount. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 HarvestPerTrip = 5;

	/** Sim ticks to mine one trip. 24 is 1.5 s at 16 Hz. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 HarvestTicks = 24;

	/** Per trip after Amount is gone. 0 destroys the node. LumineneVent uses 1. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 TricklePerTrip = 0;

	/** Worker stops this many uu from the actor origin. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 GatherStandUU = 150;

	/**
	 * Optional presentation mesh. Null uses the engine cone cluster (Jadeite) or vent (Luminene).
	 * Do not commit the uasset that points at a local mesh.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Mesh")
	TSoftObjectPtr<UStaticMesh> Mesh;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		const FName Id = !DefinitionId.IsNone() ? DefinitionId : GetFName();
		return FPrimaryAssetId(TEXT("KodResourceNodeDefinition"), Id);
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
