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
	/** Body death. Infantry, biological units, and workers. Not saved on the entity. */
	bool bOrganic = false;
	bool bHealer = false;
	int32 CargoSize = 0;
	int32 BaseArmor = 0;
	int32 EnergyMax = 0;
	int32 EnergyStart = 0;
	int32 GatherRatePermille = 1000;
	int32 PowerPoints = 0;
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
	int32 SatelliteFeed = 0;
	int32 EnergyMax = 0;
	int32 EnergyStart = 0;
	/** Per-bunker weapon. 0 means this upgrade is not a turret. Placeholder until Strategy sets it. */
	int32 AttackDamage = 0;
	int32 AttackRangeUU = 0;
	/** 0 shoots ground. 1 is air-only, same rule as the missile turret. Strategy owns the flag. */
	int32 AttacksAir = 0;
};

struct KODCORE_API FKodPowerSpec
{
	FName PowerId;
	int32 PointCost = 0;
	int32 CooldownTicks = 0;
	int32 DelayTicks = 0;
	int32 RadiusUU = 0;
	int32 Amount = 0;
	int32 DurationTicks = 0;
};

struct KODCORE_API FKodScheduledPower
{
	int32 TeamId = 0;
	FName PowerId;
	int32 X = 0;
	int32 Y = 0;
	int32 TicksLeft = 0;
	int32 Phase = 0;
	int32 RadiusUU = 0;
	int32 Amount = 0;
	int32 DurationTicks = 0;
};

struct KODCORE_API FKodTeamTech
{
	int32 InfantryWeapons = 0;
	int32 InfantryArmor = 0;
	int32 SatelliteFeed = 0;
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

	/** 0 while the site is new, 1 when construction is finished. The view does not invent this. */
	float BuildProgress() const
	{
		if (bComplete || BuildTimeTotal <= 0)
		{
			return 1.f;
		}
		return FMath::Clamp(static_cast<float>(BuildTicksDone) / static_cast<float>(FMath::Max(1, BuildTimeTotal)), 0.f, 1.f);
	}
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
	/** VG_BUNKER turret is installed. The mesh hangs on TurretMount. */
	bool bTurret = false;
	/** Shots fired from this building since spawn. Presentation only. Not hashed and not saved. */
	int32 MuzzleSerial = 0;
	TArray<FKodProductionSlot> Research;
	int32 ResearchTicksRemaining = 0;
	int32 ResearchBuildTime = 0;
	/** Extra supply from a Satellite Feed drop. 0 means none. */
	int32 BonusSupply = 0;
	/** -1 is permanent. Above 0 counts down in sim ticks. */
	int32 BonusSupplyTicksRemaining = 0;
	int32 GathererCd = 0;
	int32 SupplyCd = 0;
	int32 IsrCd = 0;
	bool bAirborne = false;
	bool bLiftMoving = false;
	int32 LiftGoalX = 0;
	int32 LiftGoalY = 0;
	int32 LiftSpeedUU = 0;
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
	/** Nonzero when a save load is respawning this id. 0 is a normal train. */
	int32 RestoreId = 0;
	int32 GatherRatePermille = 0;
	int32 OrbitCenterX = 0;
	int32 OrbitCenterY = 0;
	int32 OrbitRadiusUU = 0;
	int32 OrbitTicks = 0;
	/** Attack-move to RallyPoint after the actor is registered. A plain rally uses IssueMove. */
	bool bAttackMove = false;
	FName ActorTag;
	/** 0 is a normal train. 1 guard, 2 counter-wave, 3 dressing worker. */
	uint8 ScriptRole = 0;
	int32 ScriptHealth = 0;
	int32 ScriptDamage = 0;
	int32 ScriptRangeUU = 0;
	int32 ScriptSpeed = 0;
	int32 ScriptArmor = 0;
	int32 ScriptCooldownTicks = 0;
	int32 ScriptRegenMilli = 0;
	int32 ScriptHomeX = 0;
	int32 ScriptHomeY = 0;
	int32 ScriptLeashUU = 0;
	int32 ScriptAggroUU = 0;
};

/** Numbers copied out of Config/KodBalance/faultline_training_balance_v1.json. Hashed with the match. */
struct KODCORE_API FKodFaultlineScript
{
	int32 OutpostCount = 0;
	int32 SpawnX = 0;
	int32 SpawnY = 0;
	int32 SpawnZ = 0;
	int32 OutpostX = 0;
	int32 OutpostY = 0;
	int32 OutpostZ = 0;
	int32 LaneX = 0;
	int32 LaneY = 0;
	int32 LaneZ = 0;
	uint8 bHasLane = 0;
	int32 HomeX = 0;
	int32 HomeY = 0;
	int32 HomeZ = 0;
	int32 GuardFighters = 0;
	int32 GuardBlades = 0;
	int32 GuardWorkers = 0;
	int32 WaveFighters = 0;
	int32 WaveBlades = 0;
	int32 FighterLeashUU = 0;
	int32 FighterAggroUU = 0;
	int32 BladeLeashUU = 0;
	int32 BladeAggroUU = 0;
	int32 DelayTicks = 0;
	int32 RetreatTicks = 0;
	int32 HpPermille = 1000;
	int32 FighterHp = 1;
	int32 FighterDamage = 0;
	int32 FighterRangeUU = 0;
	int32 FighterSpeed = 0;
	int32 FighterArmor = 0;
	int32 FighterCooldownTicks = 1;
	int32 FighterRegenMilli = 0;
	int32 BladeHp = 1;
	int32 BladeDamage = 0;
	int32 BladeRangeUU = 0;
	int32 BladeSpeed = 0;
	int32 BladeArmor = 0;
	int32 BladeCooldownTicks = 1;
	int32 BladeRegenMilli = 0;
	int32 WorkerHp = 1;
	int32 WorkerSpeed = 0;
	int32 WorkerRegenMilli = 0;
};
