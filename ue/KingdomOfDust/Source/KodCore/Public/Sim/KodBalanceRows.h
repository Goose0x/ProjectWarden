#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "KodBalanceRows.generated.h"

/** Ability integers. Defaults match the Combat Stim bootstrap. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodAbilityBalanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Credits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Luminene = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 HealthCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 CooldownTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 DurationTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 MovePermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 AttackPermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 EnergyCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 SupplyBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 GatherRatePermille = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 SightUU = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 OrbitRadiusUU = 0;
};

/** Unit integers. Defaults match the slice 0 bootstrap. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodUnitBalanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 MaxHealth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Armor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 MoveSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Sight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Credits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Luminene = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Supply = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 BuildTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Biological = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Infantry = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Flyer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Healer = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 CanGather = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 CargoSize = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 EnergyMax = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 EnergyStart = 0;

	/** 1000 matches the node harvest ticks. 2000 mines twice as fast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 GatherRatePermille = 1000;

	/** Whole points awarded to the killer. 0 uses the credit cost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 PowerPoints = 0;
};

/** Weapon integers. Damage, range, and cooldown ticks. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodWeaponBalanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Damage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Range = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 CooldownTicks = 0;
};

/** Structure integers. Defaults match the slice 0 bootstrap. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodStructureBalanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 MaxHealth = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Credits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Luminene = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 BuildTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 SupplyProvided = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 FootprintX = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 FootprintY = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 MenuTier = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 TownHall = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 CanLower = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 CargoCap = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 AttackDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 AttackRangeUU = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 AttacksAir = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Extractor = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 DropOff = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Sight = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 CanLiftoff = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 LiftSpeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 PowerPoints = 0;
};

/** Vanguard power integers. Cooldowns and delays are sim ticks. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodPowerBalanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 PointCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 CooldownTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 DelayTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 RadiusUU = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Amount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 DurationTicks = 0;
};

/** Upgrade integers. Defaults match the engineering-bay placeholders. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodUpgradeBalanceRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Credits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 Luminene = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 BuildTicks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 WeaponBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 ArmorBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 SatelliteFeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 EnergyMax = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 EnergyStart = 0;

	/** VG_BUNKER_TURRET weapon. 0 on the engineering-bay rows. Placeholder until Strategy sets it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 AttackDamage = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 AttackRangeUU = 0;

	/** 0 shoots ground. 1 is air-only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Balance")
	int32 AttacksAir = 0;
};
