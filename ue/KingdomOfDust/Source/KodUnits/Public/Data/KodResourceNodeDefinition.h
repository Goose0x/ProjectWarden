#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "GameplayTagContainer.h"
#include "Sim/KodResourceTypes.h"
#include "KodResourceNodeDefinition.generated.h"

/**
 * Primary data asset for one resource-node archetype.
 * Slice 0 ids: JadeiteNode, OilSource under /Game/Warden/Data/.
 * Bootstrap NewObject fills these when the uasset is missing. PIE does not need the file.
 * Mesh empty keeps the engine-shape placeholder on AKodResourceNode.
 */
UCLASS(BlueprintType)
class KODUNITS_API UKodResourceNodeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Must match the asset name exactly (JadeiteNode, OilSource). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FName DefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FGameplayTag NodeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	EKodResourceType ResourceType = EKodResourceType::Jadeite;

	/** Starting pile. JadeiteNode 1500, OilSource 5000. Integer, sim-owned after spawn. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 Amount = 1500;

	/** One worker trip. Jadeite 5, Oil 4. Debug harvest can pass a different amount. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Economy")
	int32 HarvestPerTrip = 5;

	/**
	 * Optional presentation mesh. Null uses the engine cone (Jadeite) or cylinder (Oil).
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
