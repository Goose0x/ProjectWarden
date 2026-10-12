#pragma once

#include "CoreMinimal.h"
#include "KodAbilityTypes.generated.h"

/** What a sim ability does while its duration ticks remain. Not GAS. */
UENUM(BlueprintType)
enum class EKodAbilityEffect : uint8
{
	None,
	CombatStim
};

/**
 * Authored ability. Copied into the sim at match start.
 * Durations are integer sim ticks (16 Hz). Permille 1500 is +50%.
 */
USTRUCT(BlueprintType)
struct KODCORE_API FKodAbilitySpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	FName AbilityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	FName DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	FName Hotkey;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 Credits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 Luminene = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 HealthCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 CooldownTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 DurationTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	EKodAbilityEffect Effect = EKodAbilityEffect::None;

	/** 1000 is unchanged. 1500 is +50% move speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 MovePermille = 1000;

	/** 1000 is unchanged. 1500 is +50% attack rate (shorter cooldown). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 AttackPermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 EnergyCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 SupplyBonus = 0;

	/** Copied onto a dropped gatherer. 1000 leaves the node harvest ticks unchanged. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 GatherRatePermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 SightUU = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Ability")
	int32 OrbitRadiusUU = 0;
};
