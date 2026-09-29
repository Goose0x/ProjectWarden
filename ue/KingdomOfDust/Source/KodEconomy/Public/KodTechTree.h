#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "KodTechTree.generated.h"

USTRUCT(BlueprintType)
struct KODECONOMY_API FKodTechNode
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod")
	FGameplayTag NodeTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod")
	FGameplayTagContainer Prerequisites;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod")
	int32 CashCost = 0;
};

UCLASS(BlueprintType)
class KODECONOMY_API UKodTechTree : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Tech")
	TArray<FKodTechNode> Nodes;

	UFUNCTION(BlueprintCallable, Category = "Kod|Tech")
	bool ArePrerequisitesMet(FGameplayTag NodeTag, const FGameplayTagContainer& Unlocked) const;
};
