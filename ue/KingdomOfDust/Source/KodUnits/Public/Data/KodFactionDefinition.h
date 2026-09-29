#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KodFactionDefinition.generated.h"

class UKodBuildingDefinition;
class UKodUnitDefinition;

/**
 * Thin faction / player-template Primary Data Asset.
 * Slice 0: USA under /Game/Warden/Data/ — never reuse UKodTechTree as the faction id.
 */
UCLASS(BlueprintType)
class KODUNITS_API UKodFactionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Must match asset name exactly (e.g. USA). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FName DefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Start")
	TSoftObjectPtr<UKodBuildingDefinition> StartingBuilding;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Start")
	TArray<TSoftObjectPtr<UKodUnitDefinition>> StartingUnits;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		const FName Id = !DefinitionId.IsNone() ? DefinitionId : GetFName();
		return FPrimaryAssetId(TEXT("KodFactionDefinition"), Id);
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
