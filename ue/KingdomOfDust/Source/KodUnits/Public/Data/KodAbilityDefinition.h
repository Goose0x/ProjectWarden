#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Sim/KodAbilityTypes.h"
#include "KodAbilityDefinition.generated.h"

/** Authored ability. The sim copies FKodAbilitySpec and does not keep this asset. */
UCLASS(BlueprintType)
class KODUNITS_API UKodAbilityDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	FName AbilityId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	FName Hotkey;

	/** Command-card icon. 128 source, scaled to the button. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|UI")
	TSoftObjectPtr<UTexture2D> CommandIcon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	int32 Credits = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	int32 Luminene = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	int32 HealthCost = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	int32 CooldownTicks = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	int32 DurationTicks = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	EKodAbilityEffect Effect = EKodAbilityEffect::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	int32 MovePermille = 1000;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Ability")
	int32 AttackPermille = 1000;

	FKodAbilitySpec ToSpec() const
	{
		FKodAbilitySpec Spec;
		Spec.AbilityId = AbilityId;
		Spec.DisplayName = DisplayName.IsEmpty() ? AbilityId : FName(*DisplayName.ToString());
		Spec.Hotkey = Hotkey;
		Spec.Credits = Credits;
		Spec.Luminene = Luminene;
		Spec.HealthCost = HealthCost;
		Spec.CooldownTicks = CooldownTicks;
		Spec.DurationTicks = DurationTicks;
		Spec.Effect = Effect;
		Spec.MovePermille = MovePermille;
		Spec.AttackPermille = AttackPermille;
		return Spec;
	}

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		const FName Id = !AbilityId.IsNone() ? AbilityId : GetFName();
		return FPrimaryAssetId(TEXT("KodAbilityDefinition"), Id);
	}
};
