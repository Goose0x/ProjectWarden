#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "KodGeneralPowerSet.generated.h"

class UGameplayAbility;

USTRUCT(BlueprintType)
struct KODGENERALS_API FKodPowerEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod")
	FGameplayTag PowerTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod")
	TSubclassOf<UGameplayAbility> AbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod")
	int32 RankRequired = 1;
};

/** DataAsset listing GA classes / tags for a commander loadout. */
UCLASS(BlueprintType)
class KODGENERALS_API UKodGeneralPowerSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Powers")
	TArray<FKodPowerEntry> Powers;
};
