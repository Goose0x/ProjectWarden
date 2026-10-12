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
};

struct KODCORE_API FKodPlacementResult
{
	bool bValid = false;
	FName Reason;
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
};
