#pragma once

#include "CoreMinimal.h"
#include "Sim/KodEntityId.h"
#include "Sim/KodResourceTypes.h"

/** 200 uu cells. One SC2 build cell at this project's unit scale. */
namespace KodBuildGrid
{
	inline constexpr int32 CellUU = 200;

	inline int32 CellOf(float World)
	{
		return FMath::FloorToInt(World / static_cast<float>(CellUU));
	}

	inline float CellCenter(int32 Cell)
	{
		return (static_cast<float>(Cell) + 0.5f) * static_cast<float>(CellUU);
	}
}

/** Placeable archetype. Filled from a building DataAsset. The sim does not load it. */
struct KODCORE_API FKodBuildSpec
{
	FName TypeId;
	FName DisplayId;
	int32 FootprintX = 1;
	int32 FootprintY = 1;
	FKodTrainCost Cost;
	float MaxHealth = 1000.f;
	int32 SupplyProvided = 0;
	bool bIsDropOff = false;
	bool bExtractor = false;
	TArray<FName> Trains;
	/** Completed structure required before this can be placed. Empty means none. */
	FName RequiresId;
	/** 0 basic build menu, 1 advanced. */
	int32 MenuTier = 0;
	bool bTownHall = false;
	bool bCanLower = false;
	int32 CargoCap = 0;
	int32 AttackDamage = 0;
	int32 AttackRangeUU = 0;
	bool bAttacksAir = false;
	/** VG_BUNKER is 1. Placeholder until the Director stamps it. */
	int32 Armor = 0;
	bool bArmored = false;
	bool bStructure = false;
	bool bCanLiftoff = false;
	int32 LiftSpeedUU = 0;
	int32 PowerPoints = 0;
};

struct KODCORE_API FKodPlacementResult
{
	bool bValid = false;
	FName Reason;
	/** Set when Reason is Requires. The HUD prints this name. */
	FName RequiredId;
	/** One byte per footprint cell, row-major. 1 is open, 0 is blocked. */
	TArray<uint8> CellOk;
	int32 CellX = 0;
	int32 CellY = 0;
	FKodEntityId VentId;
	FVector WorldCenter = FVector::ZeroVector;
};

/** Actor spawn requested by a confirmed placement. Drained after the sim step. */
struct KODCORE_API FKodPendingBuildingSpawn
{
	int32 StructureId = 0;
	FName TypeId;
	int32 TeamId = 0;
	FVector Location = FVector::ZeroVector;
	int32 FootprintX = 1;
	int32 FootprintY = 1;
	int32 YawDegrees = 0;
	/** Nonzero when a save load is respawning this structure id. */
	int32 RestoreId = 0;
};
