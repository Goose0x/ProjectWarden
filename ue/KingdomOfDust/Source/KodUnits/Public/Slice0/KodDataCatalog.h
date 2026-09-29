#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "KodDataCatalog.generated.h"

class UPrimaryDataAsset;

/**
 * Soft map of bare Warden ids → PrimaryDataAsset instances.
 * Bootstrap registers here; cooked /Game/Warden/Data/<Id> assets take precedence when loadable.
 */
UCLASS(BlueprintType)
class KODUNITS_API UKodDataCatalog : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|Warden")
	void RegisterAsset(FName DefinitionId, UPrimaryDataAsset* Asset);

	UFUNCTION(BlueprintCallable, Category = "Kod|Warden")
	UPrimaryDataAsset* FindAsset(FName DefinitionId) const;

	template<typename T>
	T* Find(FName DefinitionId) const
	{
		return Cast<T>(FindAsset(DefinitionId));
	}

	UFUNCTION(BlueprintPure, Category = "Kod|Warden")
	int32 Num() const { return Assets.Num(); }

	const TMap<FName, TObjectPtr<UPrimaryDataAsset>>& GetAssets() const { return Assets; }

protected:
	UPROPERTY()
	TMap<FName, TObjectPtr<UPrimaryDataAsset>> Assets;
};
