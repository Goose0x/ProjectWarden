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
	FName RequiresId;
	bool bBiological = false;
	bool bInfantry = false;
	bool bFlyer = false;
	bool bHealer = false;
	int32 CargoSize = 0;
	int32 BaseArmor = 0;
	int32 EnergyMax = 0;
	int32 EnergyStart = 0;
};

/** Researched at a building. Level 1 placeholders. The sim does not load an asset. */
struct KODCORE_API FKodUpgradeSpec
{
	FName UpgradeId;
	FName DisplayId;
	FName BuildingId;
	int32 Credits = 0;
	int32 Luminene = 0;
	int32 BuildTicks = 0;
	int32 WeaponBonus = 0;
	int32 ArmorBonus = 0;
};

struct KODCORE_API FKodTeamTech
{
	int32 InfantryWeapons = 0;
	int32 InfantryArmor = 0;
};

struct KODCORE_API FKodStructureSpec
{
	FName DefinitionId;
	FName DisplayId;
	int32 TeamId = 0;
	int32 SupplyProvided = 0;
	bool bIsDropOff = false;
	int32 DropOffStandUU = 360;
	int32 FootprintX = 5;
	int32 FootprintY = 5;
	float MaxHealth = 2500.f;
	TArray<FName> Trains;
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
	FKodEntityId RallyUnit;

	int32 CellX = 0;
	int32 CellY = 0;
	int32 FootprintX = 1;
	int32 FootprintY = 1;
	int32 BuildTimeTotal = 0;
	int32 BuildTicksDone = 0;
	bool bComplete = true;
	bool bActivelyBuilding = false;
	int32 LastProgressBucket = -1;
	FKodEntityId BuilderId;
	FKodEntityId VentId;
	bool bExtractor = false;
	int32 PaidCredits = 0;
	int32 PaidLuminene = 0;
	float MaxHealth = 0.f;
	FName DisplayId;
	TArray<FName> Trains;
	FVector StandPoint = FVector::ZeroVector;
	bool bLowered = false;
	bool bCanLower = false;
	int32 CargoCap = 0;
	TArray<int32> Cargo;
	int32 AttackDamage = 0;
	int32 AttackRangeUU = 0;
	bool bAttacksAir = false;
	TArray<FKodProductionSlot> Research;
	int32 ResearchTicksRemaining = 0;
	int32 ResearchBuildTime = 0;
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
	FKodEntityId RallyUnit;
};
