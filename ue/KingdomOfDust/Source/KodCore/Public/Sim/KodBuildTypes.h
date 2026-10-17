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

	/** Even footprints sit on a grid line. Odd footprints sit on a cell centre, 100 uu off that line. */
	inline bool IsEvenFootprint(int32 Cells)
	{
		return (FMath::Max(1, Cells) & 1) == 0;
	}

	/** Nearest legal pivot on one axis. Even N lands on a multiple of 200. Odd N lands on a cell centre. */
	inline float SnapPivot(float World, int32 Cells)
	{
		const float Cell = static_cast<float>(CellUU);
		if (IsEvenFootprint(Cells))
		{
			return static_cast<float>(FMath::RoundToInt(World / Cell)) * Cell;
		}
		return CellCenter(CellOf(World));
	}

	/**
	 * Min cell of the N cells under a pivot-centred box.
	 * The box is half-open, so the far edge is not an extra cell.
	 * Even: the pivot is the shared edge, and half the cells sit on each side.
	 */
	inline int32 OriginCell(float Pivot, int32 Cells)
	{
		const int32 Size = FMath::Max(1, Cells);
		if (IsEvenFootprint(Size))
		{
			const int32 Line = FMath::RoundToInt(Pivot / static_cast<float>(CellUU));
			return Line - Size / 2;
		}
		return CellOf(Pivot) - Size / 2;
	}

	/** Actor origin for a footprint whose min cell is Origin. Matches SnapPivot for that footprint. */
	inline float PivotForOrigin(int32 Origin, int32 Cells)
	{
		const int32 Size = FMath::Max(1, Cells);
		return (static_cast<float>(Origin) + static_cast<float>(Size) * 0.5f) * static_cast<float>(CellUU);
	}

	/** Local offset from the pivot to the centre of cell Index inside the footprint. */
	inline float CellLocalOffset(int32 Index, int32 Cells)
	{
		const int32 Size = FMath::Max(1, Cells);
		return (static_cast<float>(Index) + 0.5f - static_cast<float>(Size) * 0.5f) * static_cast<float>(CellUU);
	}

	inline bool CoversCell(int32 Cell, int32 Origin, int32 Cells)
	{
		const int32 Size = FMath::Max(1, Cells);
		return Cell >= Origin && Cell < Origin + Size;
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
