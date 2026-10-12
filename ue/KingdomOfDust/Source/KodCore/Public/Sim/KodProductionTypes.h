#pragma once

#include "CoreMinimal.h"
#include "Sim/KodEntityId.h"
#include "Sim/KodResourceTypes.h"

/** Registered from a unit DataAsset. The sim does not load the asset. */
struct KODCORE_API FKodTrainSpec
{
	FName UnitId;
	FName DisplayId;
	FKodTrainCost Cost;
	bool bCanGather = false;
};

struct KODCORE_API FKodStructureSpec
{
	FName DefinitionId;
	int32 TeamId = 0;
	int32 SupplyProvided = 0;
	bool bIsDropOff = false;
	int32 DropOffStandUU = 360;
};

struct KODCORE_API FKodProductionSlot
{
	FName UnitId;
	FName DisplayId;
	int32 Credits = 0;
	int32 Luminene = 0;
	int32 Supply = 0;
	int32 BuildTime = 0;
	bool bStarted = false;
};

struct KODCORE_API FKodStructureState
{
	FKodEntityId Id;
	int32 TeamId = 0;
	FName DefinitionId;
	FVector Position = FVector::ZeroVector;
	FVector ExitPoint = FVector::ZeroVector;
	int32 SupplyProvided = 0;
	bool bIsDropOff = false;
	TArray<FKodProductionSlot> Slots;
	int32 ActiveTicksRemaining = 0;
	int32 ActiveBuildTime = 0;
	bool bSupplyBlocked = false;
	FVector RallyPoint = FVector::ZeroVector;
	FKodEntityId RallyNode;
	bool bRallyNode = false;
};

struct KODCORE_API FKodSupplyState
{
	int32 Used = 0;
	int32 Cap = 0;
};

/** Filled when a train finishes. Presentation spawns the actor after the sim step. */
struct KODCORE_API FKodPendingUnitSpawn
{
	FName UnitId;
	int32 TeamId = 0;
	int32 SupplyCost = 0;
	int32 StructureId = 0;
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	FVector RallyPoint = FVector::ZeroVector;
	FKodEntityId RallyNode;
	bool bRallyNode = false;
};
