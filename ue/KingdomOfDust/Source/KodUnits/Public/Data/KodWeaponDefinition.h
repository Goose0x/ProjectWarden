#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "KodWeaponDefinition.generated.h"

/**
 * Primary data asset for a weapon archetype.
 * Runtime MUST load this DA — do not hardcode damage/range on actors.
 * Slice 0 id law example: FName "RangerRifle" under /Game/Warden/Data/.
 */
UCLASS(BlueprintType)
class KODUNITS_API UKodWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * Must match the asset file / Primary Asset Name exactly (e.g. RangerRifle).
	 * Bootstrap and editor Save As both stamp this.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FName DefinitionId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Identity")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Weapon")
	float Damage = 10.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Weapon")
	float Range = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Weapon")
	float CooldownSeconds = 1.f;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		const FName Id = !DefinitionId.IsNone() ? DefinitionId : GetFName();
		return FPrimaryAssetId(TEXT("KodWeaponDefinition"), Id);
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
