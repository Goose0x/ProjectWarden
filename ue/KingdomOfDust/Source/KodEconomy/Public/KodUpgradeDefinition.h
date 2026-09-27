#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "KodUpgradeDefinition.generated.h"

UCLASS(BlueprintType)
class KODECONOMY_API UKodUpgradeDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Upgrade")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Upgrade")
	FGameplayTag UpgradeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Upgrade")
	int32 DustCost = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Upgrade")
	FGameplayTagContainer RequiredTech;
};
