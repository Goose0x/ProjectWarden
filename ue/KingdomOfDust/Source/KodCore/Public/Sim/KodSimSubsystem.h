#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Sim/KodEntityId.h"
#include "Sim/KodBuildTicks.h"
#include "Sim/KodResourceTypes.h"
#include "Sim/KodProductionTypes.h"
#include "Sim/KodBuildTypes.h"
#include "Sim/KodAbilityTypes.h"
#include "Sim/KodBalanceRows.h"
#include "Sim/KodCheatRows.h"
#include "KodSimSubsystem.generated.h"

class AActor;

/** Presentation binds these. Listeners must not write sim state or the idle hash. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FKodOnUnitFired, AActor*, Attacker, AActor*, Target, int32, SimTick);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FKodOnUnitHit, AActor*, Attacker, AActor*, Target, float, Health, int32, TargetId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKodOnUnitKilled, AActor*, Target, int32, TargetId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKodOnTrainCompleted, FVector, Location, int32, TeamId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FKodOnCue, FName, CueId, FVector, Location, bool, bAtLocation);

/** Floating combat or resource text. Presentation only. Not a sim field. */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FKodJuiceTextEvent, FVector, int32, uint8);

UENUM(BlueprintType)
enum class EKodSimOrderType : uint8
{
	None,
	Move,
	Attack,
	Gather,
	AttackMove,
	Hold,
	Patrol,
	Follow,
	Guard
};

/** One shift-queued order. Integers only so the idle hash stays stable. */
USTRUCT()
struct KODCORE_API FKodQueuedOrder
{
	GENERATED_BODY()

	UPROPERTY()
	EKodSimOrderType Order = EKodSimOrderType::None;

	UPROPERTY()
	int32 X = 0;

	UPROPERTY()
	int32 Y = 0;

	UPROPERTY()
	int32 TargetId = 0;

	UPROPERTY()
	int32 PathId = 0;

	UPROPERTY()
	int32 OffsetX = 0;

	UPROPERTY()
	int32 OffsetY = 0;
};

/** Sim-owned entity state. Actors interpolate presentation from this — not CharacterMovement. */
USTRUCT(BlueprintType)
struct KODCORE_API FKodSimEntityState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId Id;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FName DefinitionId;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float YawDegrees = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float Health = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float MaxHealth = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float MoveSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float AcceptanceRadius = 50.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	EKodSimOrderType Order = EKodSimOrderType::None;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FVector MoveTarget = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId AttackTarget;

	/**
	 * Attacker to retaliate against after a hit taken while Idle or AttackMove.
	 * Not an order. Idle consumes it. Attack-move keeps it while the attacker is visible.
	 * Not part of ComputeIdleHash. An explicit Move / Attack / Stop clears it.
	 * Workers never receive one. They attack only on an explicit attack order.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId RetaliateTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float WeaponDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float WeaponRange = 0.f;

	/** Authored seconds. The sim fires from WeaponCooldownTicks. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float WeaponCooldownSeconds = 0.f;

	/** Integer shot cadence. Stim scales this. Hashed. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 WeaponCooldownTicks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 CooldownTicksRemaining = 0;

	/** Legacy seconds mirror. The fire gate is CooldownTicksRemaining. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float CooldownRemaining = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	bool bMobile = true;

	/**
	 * Owner team. Slice 0 local player is 0; a different id is hostile.
	 * Not part of ComputeIdleHash (ids, quantized pose, HP, order only).
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 TeamId = 0;

	/** Dozer. Copied from the unit definition. Unarmed gatherers are not idle-acquire targets. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	bool bCanGather = false;

	/**
	 * Gather loop. Integers only. Hashed while the worker is live, including zeros,
	 * so a cancelled order and a finished trip do not alias each other.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	EKodGatherPhase GatherPhase = EKodGatherPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	FKodEntityId GatherNode;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	FKodEntityId GatherDropOff;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	EKodResourceType GatherType = EKodResourceType::Jadeite;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 CargoJadeite = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 CargoLuminene = 0;

	/** True when the carried amount was a depleted-vent trickle, not a pile withdrawal. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	bool bCargoFromTrickle = false;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 HarvestTicksRemaining = 0;

	/** Ticks spent without a pose change on the current leg. Reset on progress or a new phase. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 GatherStallTicks = 0;

	/** One automatic retarget is allowed per leg. A second stall aborts the order. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	bool bGatherStallRetargeted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 SupplyCost = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	bool bSupplyCounted = false;

	/** Incomplete structure this worker is constructing. Cleared by any other order. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	FKodEntityId BuildSite;

	/** Active sim ability. Empty when no buff is running. Hashed as the id string plus integer ticks. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Ability")
	FName ActiveAbilityId;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Ability")
	int32 AbilityTicksRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Ability")
	int32 AbilityCooldownTicks = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Ability")
	int32 AbilityMovePermille = 1000;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Ability")
	int32 AbilityAttackPermille = 1000;

	/** 1000 is one harvest tick per authored node tick. 2000 finishes the trip in half the ticks. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Economy")
	int32 GatherRatePermille = 1000;

	/** ISR platform orbit. Zero on every other entity. Hashed. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Orbital")
	int32 OrbitAngle = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Orbital")
	int32 OrbitCenterX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Orbital")
	int32 OrbitCenterY = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Orbital")
	int32 OrbitRadiusUU = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Orbital")
	int32 OrbitTicksRemaining = 0;

	/** Collision radius from the unit definition, in uu. Hashed. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 BodyRadiusUU = 34;

	/** Ticks after the shot starts before damage lands. Hashed. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 WeaponBackswingTicks = 2;

	/** Windup left. A move order clears this without dealing damage. Hashed. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 BackswingTicksRemaining = 0;

	/** Follow or guard subject. Hashed. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId FollowTarget;

	/** Other end of a patrol. Hashed as quantized XY. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FVector PatrolAnchor = FVector::ZeroVector;

	/** Shared path id. 0 means steer straight at MoveTarget. Not a world position. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 PathId = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 PathCursor = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 PathOffsetX = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 PathOffsetY = 0;

	/** Shift-queued steps. Hashed in order. Plain property: FKodQueuedOrder is not BlueprintType. */
	UPROPERTY()
	TArray<FKodQueuedOrder> OrderQueue;

	/** Sight radius in uu. Hashed. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	int32 SightRadiusUU = 1600;

	/**
	 * True when IssueOrder started this Attack. Auto-acquire leaves it false.
	 * An explicit attack that loses sight becomes an attack-move. Hashed.
	 */
	UPROPERTY()
	bool bExplicitAttack = false;

	/** Base armour plus the definition. Upgrades are applied when the shot lands. Hashed. */
	UPROPERTY()
	int32 ArmorPoints = 0;

	UPROPERTY()
	int32 Energy = 0;

	UPROPERTY()
	int32 EnergyMax = 0;

	/** Remainder for 0.7875 energy per second. Hashed. */
	UPROPERTY()
	int32 EnergyRegenAcc = 0;

	/** Remainder for 12.6 HP per second. Hashed. */
	UPROPERTY()
	int32 HealAcc = 0;

	UPROPERTY()
	bool bAutocastHeal = false;

	UPROPERTY()
	bool bBiological = false;

	UPROPERTY()
	bool bInfantry = false;

	UPROPERTY()
	bool bHealer = false;

	UPROPERTY()
	bool bFlyer = false;

	UPROPERTY()
	int32 CargoSize = 0;

	/** Bunker this unit is inside. Hashed. */
	UPROPERTY()
	FKodEntityId InsideBunker;

	/** Bunker this unit is walking to. Hashed. */
	UPROPERTY()
	FKodEntityId LoadTarget;

	bool IsMoving() const
	{
		return Order == EKodSimOrderType::Move
			|| Order == EKodSimOrderType::AttackMove
			|| Order == EKodSimOrderType::Patrol
			|| Order == EKodSimOrderType::Follow
			|| Order == EKodSimOrderType::Guard;
	}
	bool HasPendingOrder() const { return Order != EKodSimOrderType::None; }
	bool IsCarryingCargo() const { return CargoJadeite > 0 || CargoLuminene > 0; }
};

/** Local team 0. Victory means team 1's structures are gone. Defeat means team 0's are gone. */
enum class EKodMatchOutcome : uint8
{
	None,
	Victory,
	Defeat
};

/**
 * Fixed-step sim authority (SimHz=16). Registry-only is NOT enough for Slice 0.
 * Owns pose + orders; presentation actors interpolate. NavMesh forbidden this slice.
 */
UCLASS()
class KODCORE_API UKodSimSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	FKodEntityId RegisterEntity(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	void UnregisterEntity(FKodEntityId Id);
	/** EndPlay passes the dying actor so a load respawn can keep the restored id. */
	void UnregisterEntityIfActor(FKodEntityId Id, AActor* Actor);
	bool IsTestIsolation() const { return bTestIsolation; }
	bool IsLoadRespawn() const { return bLoadRespawn; }

	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	AActor* ResolveEntity(FKodEntityId Id) const;

	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	FKodEntityId FindIdForActor(AActor* Actor) const;

	/** Initialize / refresh sim state from DataAsset-driven values (HP from DA only). */
	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	void ConfigureEntity(
		FKodEntityId Id,
		FName DefinitionId,
		float MaxHealth,
		float MoveSpeed,
		float AcceptanceRadius,
		bool bMobile,
		float WeaponDamage,
		float WeaponRange,
		float WeaponCooldownSeconds,
		int32 TeamId = 0,
		bool bCanGather = false,
		int32 BodyRadiusUU = 34,
		int32 SightRadiusUU = 1600);

	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	void IssueMove(FKodEntityId Id, FVector WorldLocation);

	/** Move to a point and shoot enemies along the way. Structures are last, after units. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	void IssueAttackMove(FKodEntityId Id, FVector WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	void IssueAttack(FKodEntityId Id, FKodEntityId TargetId);

	/** bForced allows a friendly target, including a friendly structure. */
	void IssueAttackForced(FKodEntityId Id, FKodEntityId TargetId, bool bForced);

	/**
	 * Replace the current order, or append it when bQueued and an order is already running.
	 * SharedPathId 0 plans a path for this unit. A positive id is one path shared by the group.
	 */
	void IssueOrder(
		FKodEntityId Id,
		EKodSimOrderType Kind,
		FVector Point,
		FKodEntityId Target,
		bool bQueued,
		int32 SharedPathId = 0,
		int32 OffsetX = 0,
		int32 OffsetY = 0);

	/** Grid A* around structure footprints. 0 means go straight. TeamId walks its own lowered depots. */
	int32 PlanMovePath(FVector From, FVector To, int32 TeamId = -1);

	/** 0 unexplored, 1 explored, 2 visible. Local and AI teams share this grid. */
	uint8 GetTeamVision(int32 TeamId, float WorldX, float WorldY) const;

	bool CanTeamSeeWorld(int32 TeamId, float WorldX, float WorldY) const;

	void CopySeenStructureBlips(int32 TeamId, TArray<int32>& OutTeam, TArray<int32>& OutX, TArray<int32>& OutY, TArray<int32>& OutFootX, TArray<int32>& OutFootY) const;

	/** Height layer. High ground and ramp cells are 1. Flat ground is 0. */
	int32 GetMapCellHeight(int32 CellX, int32 CellY) const;
	void RegisterRampCell(int32 CellX, int32 CellY);
	bool IsRampCell(int32 CellX, int32 CellY) const;
	/** Sorted unique cliff cells from map_features. Hashed. */
	void SetCliffBlockCells(TArray<int64> Cells);
	bool IsDressBlockCell(int32 CellX, int32 CellY) const;
	/** LOS scrub. Blocks fog vision through the cell. Hashed. */
	void SetVisionBlockCells(TArray<int64> Cells);
	bool IsVisionBlockCell(int32 CellX, int32 CellY) const;
	bool VisionRayHitsBlock(int32 X0, int32 Y0, int32 X1, int32 Y1) const;
	/** Sorted unique plateau cells. Low ground cannot see them. Not saved. */
	void SetHighGroundCells(TArray<int64> Cells);
	bool IsHighGroundCell(int32 CellX, int32 CellY) const;
	/** Immobile rock. Footprint blocks walk while Health > 0. HP and armour are hashed with the entity. */
	FKodEntityId RegisterMapDestructible(AActor* Actor, int32 MinX, int32 MinY, int32 MaxX, int32 MaxY, float HP, int32 Armor);
	void SetLayoutSockets(const TArray<FVector2D>& Sockets);
	bool LayoutSocketsMatch() const;
	/** True when a Depot or Barracks closes this ramp for ForTeam. */
	bool IsRampWalled(int32 CellX, int32 CellY, int32 ForTeam) const;

	void SetAiCheatVision(bool bEnabled) { bAiCheatVision = bEnabled; }
	bool HasAiCheatVision() const { return bAiCheatVision; }

	bool IsPathCellClear(int32 X0, int32 Y0, int32 X1, int32 Y1, int32 TeamId = -1) const;

	/**
	 * Resource nodes are never targets. A friendly drop-off is a rally, not a target.
	 * Any other live combat state, including an enemy command center, can be shot.
	 */
	bool IsAttackable(FKodEntityId TargetId, int32 AttackerTeam) const;

	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	void IssueStop(FKodEntityId Id);

	UFUNCTION(BlueprintPure, Category = "Kod|Sim")
	bool TryGetState(FKodEntityId Id, FKodSimEntityState& OutState) const;

	UFUNCTION(BlueprintPure, Category = "Kod|Sim")
	FVector GetPresentationPosition(FKodEntityId Id, float Alpha) const;

	UFUNCTION(BlueprintPure, Category = "Kod|Sim")
	int32 GetPendingOrderCount() const;

	UFUNCTION(BlueprintPure, Category = "Kod|Sim")
	bool IsWorldIdle() const;

	/**
	 * Stable when no units moving and no orders pending.
	 * Hashes entity ids, quantized positions, HP, order, and gather state (phase, node, drop-off,
	 * cargo, harvest ticks, stall, supply cost), then each team bank (Jadeite, Luminene) and supply
	 * used/cap, each resource node (id, type, remaining, quantized position), each drop-off
	 * (id, team, quantized position), and each structure queue (slots, progress, rally).
	 * Those fields are integers. TeamId, RetaliateTarget, visual bob, visual yaw, cargo mesh tint,
	 * and other presentation (flash, tracer, HP bar, death sink, resource readout) are not hashed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	FString ComputeIdleHash() const;

	/**
	 * Replace one team's bank. Match start only — gameplay uses Spend and Deposit.
	 * Team 0 is seeded in Initialize from StartingJadeite / StartingLuminene (50 / 0).
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void SetTeamBank(int32 TeamId, int32 Jadeite, int32 Luminene);

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	bool TryGetBank(int32 TeamId, FKodResourceCost& OutBank) const;

	/** True when the team holds at least Cost. Negative cost fields count as zero. */
	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	bool CanAfford(int32 TeamId, FKodResourceCost Cost) const;

	/**
	 * Subtract Cost from the team bank. Fails and leaves the bank unchanged when CanAfford is false.
	 * Logs KodEcon Spend on success and KodEcon Spend Reject when it fails (zero-cost is silent success).
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	bool Spend(int32 TeamId, FKodResourceCost Cost);

	/**
	 * Add non-negative amounts into the team bank (creates the bank at zero first if it is missing).
	 * Logs KodEcon Deposit Team=.. J=.. L=..
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void Deposit(int32 TeamId, FKodResourceCost Amount);

	/**
	 * Register a placed or spawned node. Shares the entity id space with units but is not a combat state,
	 * so auto-acquire, hitscan, and HP bars ignore it. Amount <= 0 is rejected.
	 * Numbers come from FKodResourceNodeSpec (the DataAsset).
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	FKodEntityId RegisterResourceNode(AActor* Actor, const FKodResourceNodeSpec& Spec);

	/**
	 * PROXY_CC or a Command Center. Shares the entity id space. Not selectable and not attackable.
	 * StandUU is how close a worker must get before the cargo hits the bank.
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	FKodEntityId RegisterDropOff(AActor* Actor, int32 TeamId, int32 StandUU);

	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void UnregisterDropOff(FKodEntityId Id);

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	bool IsDropOff(FKodEntityId Id) const;

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	FKodEntityId FindNearestDropOff(int32 TeamId, FVector Origin) const;

	/** Drop the sim record. Does not destroy the actor (EndPlay uses this). */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void UnregisterResourceNode(FKodEntityId Id);

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	bool IsResourceNode(FKodEntityId Id) const;

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	bool TryGetResourceNode(FKodEntityId Id, FKodResourceNodeState& OutNode) const;

	/** 2D nearest node with Remaining > 0. Ties go to the lowest entity id. Origin is not hashed. */
	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	FKodEntityId FindNearestResourceNode(FVector Origin) const;

	/**
	 * 2D nearest node of Type. bRequireRemaining skips empty piles.
	 * A depleted vent (Remaining 0, TricklePerTrip > 0) is a candidate only when bRequireRemaining is false.
	 * Ties go to the lowest entity id.
	 */
	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	FKodEntityId FindNearestResourceNodeOfType(FVector Origin, EKodResourceType Type, bool bRequireRemaining) const;

	/**
	 * Order a worker onto a node. RMB ground (IssueMove) and Stop cancel and refund a pile withdrawal.
	 * The loop is fixed-step: walk, harvest ticks, carry, drop-off, repeat.
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	bool IssueGather(FKodEntityId WorkerId, FKodEntityId NodeId);

	/**
	 * Take up to Amount from the node and Deposit it into TeamId.
	 * While Remaining > 0, Amount is clamped to Remaining. A depleted vent with a trickle yields
	 * TricklePerTrip instead of Amount. A jadeite pile at 0 is destroyed.
	 * Logs Deposit, then KodEcon Node Id=.. Type=.. Remaining=.., and KodEcon NodeDepleted Id=.. once.
	 * Returns the amount taken (0 on a miss).
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	int32 HarvestNode(FKodEntityId Id, int32 Amount, int32 TeamId);

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	bool TryGetSupply(int32 TeamId, int32& OutUsed, int32& OutCap) const;

	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void AccountUnitSupply(FKodEntityId Id, int32 Cost, bool bAlreadyPaid = false);

	void RegisterTrainSpec(const FKodTrainSpec& Spec);
	FKodEntityId RegisterStructure(AActor* Actor, const FKodStructureSpec& Spec);
	bool IsStructure(FKodEntityId Id) const;
	bool TryGetStructure(FKodEntityId Id, FKodStructureState& OutState) const;
	bool EnqueueTrain(FKodEntityId StructureId, FName UnitId);
	bool CancelLastTrain(FKodEntityId StructureId);
	bool CancelTrainAt(FKodEntityId StructureId, int32 SlotIndex);
	bool TryGetTrainSpec(FName UnitId, FKodTrainSpec& OutSpec) const;
	bool TryGetBuildSpec(FName TypeId, FKodBuildSpec& OutSpec) const;
	void RegisterUnitAbility(FName UnitDefinitionId, const FKodAbilitySpec& Spec);
	bool TryGetAbilitySpec(FName AbilityId, FKodAbilitySpec& OutSpec) const;
	bool UnitHasAbility(FName UnitDefinitionId, FName AbilityId) const;
	bool UseAbility(FKodEntityId UnitId, FName AbilityId);
	void SetStructureRally(FKodEntityId StructureId, FVector GroundPoint, FKodEntityId NodeId, FKodEntityId UnitId = FKodEntityId());
	void ConsumePendingSpawns(TArray<FKodPendingUnitSpawn>& OutSpawns);
	/** Presentation hook. Does not start the next slot or change supply. */
	void NotifyTrainSpawned(int32 StructureId);

	void RegisterBuildSpec(const FKodBuildSpec& Spec);
	FKodPlacementResult QueryPlacement(int32 TeamId, FName TypeId, FVector CursorWorld) const;
	bool TryPlaceStructure(int32 TeamId, FName TypeId, FVector CursorWorld, FKodEntityId WorkerId);
	bool ResumeConstruction(FKodEntityId StructureId, FKodEntityId WorkerId);
	bool CancelConstruction(FKodEntityId StructureId);
	bool TryGetPrimaryTrain(FKodEntityId StructureId, FName& OutUnitId) const;
	void SetPlayableWorldBox(const FBox& WorldBox);
	bool TryGetPlayableCellBounds(int32& OutMinX, int32& OutMinY, int32& OutMaxX, int32& OutMaxY) const;
	/** True when every node and its gather-stand ring sit at least InsetCells inside the playable box. */
	bool AreResourcePointsInset(int32 InsetCells) const;
	void BindStructureActor(FKodEntityId Id, AActor* Actor);
	void ConsumePendingBuildingSpawns(TArray<FKodPendingBuildingSpawn>& OutSpawns);
	/**
	 * A structure footprint blocks the cell.
	 * bIgnoreLowered is for movement only: a lowered depot stays walkable.
	 * Placement leaves the flag false, so the same footprint stays occupied.
	 */
	bool IsCellBlocked(int32 CellX, int32 CellY, int32 TeamId = -1, bool bIgnoreLowered = false) const;

	/** Default 50. L_Slice0 overwrites this from AKodSlice0GameMode at StartPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 StartingJadeite = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 StartingLuminene = 0;

	UFUNCTION(BlueprintPure, Category = "Kod|Sim")
	int64 GetSimTickIndex() const { return SimTickIndex; }

	/** Team 1 opponent. Off stops new orders. Units already moving keep those orders. */
	void SetAiEnabled(bool bEnabled);
	bool IsAiEnabled() const { return bAiEnabled; }
	/**
	 * Offline and campaign only. Team 0 runs the same sim-tick AI as team 1.
	 * Not hashed and not saved. kod.Autoplay=1
	 */
	void SetPlayerAutoplay(bool bEnabled);
	bool IsPlayerAutoplay() const { return bPlayerAutoplay; }
	/** Exact sim steps. Wall-clock time is not involved. */
	void AdvanceSimTicks(int32 Ticks);
	/** 1 matches the player's 50 Credit start. Each step above 1 adds 100 starting Credits and grows waves faster. */
	void SetAiDifficulty(int32 Difficulty);
	int32 GetAiDifficulty() const { return AiDifficulty; }
	void SetAiSeeded(bool bSeeded);
	bool IsAiSeeded() const { return bAiSeeded; }
	/** Player spawn. Waves and the scout attack-move here when nothing has been seen. Hashed. */
	void SetEnemyStart(FVector WorldLocation);
	/** Quantized yaw, degrees. Buildings face this and do not rotate when placed. */
	void SetTeamFacing(int32 TeamId, int32 YawDegrees);
	int32 GetTeamFacing(int32 TeamId) const;
	bool HasCompletedStructure(int32 TeamId, FName TypeId) const;
	void RegisterUpgradeSpec(const FKodUpgradeSpec& Spec);
	bool TryGetUpgradeSpec(FName UpgradeId, FKodUpgradeSpec& OutSpec) const;
	bool EnqueueResearch(FKodEntityId StructureId, FName UpgradeId);
	bool ToggleDepot(FKodEntityId StructureId);
	bool LoadUnitIntoBunker(FKodEntityId UnitId, FKodEntityId BunkerId);
	bool UnloadUnit(FKodEntityId BunkerId, int32 UnitId);
	bool UnloadAll(FKodEntityId BunkerId);
	void SetHealAutocast(FKodEntityId UnitId, bool bEnabled);
	bool CastHeal(FKodEntityId UnitId);
	void RaiseCue(FName CueId, FVector Location, bool bAtLocation);
	/** Armour and a structure gun. The shot comes from the structure's turret fields, not a mobile unit. */
	void TuneStructureCombat(FKodEntityId Id, int32 Armor, float WeaponDamage, float WeaponRange);
	/** Team 0 damage. Area-throttled under-attack alert. Hashed. */
	void NotifyOwnedDamage(const FKodSimEntityState& Target);
	/** 0 is Vanguard, 1 is Scinder. The other faction is the enemy. Hashed and saved. */
	void SetPlayerFaction(int32 Faction);
	int32 GetPlayerFaction() const { return PlayerFaction; }
	bool IsFaultlineTraining() const { return bFaultlineTraining; }
	/**
	 * Arms the tutorial from the balance file. OutpostCount 0 logs an error and never spawns.
	 * Guards, the delayed counter-wave, leash, and retreat all run in the sim tick. Hashed and saved.
	 */
	void ArmFaultlineScript(const FKodFaultlineScript& Script);
	/** Structure HP, armour, and always-on regen. Does not clear a turret's attack fields. */
	void SetStructureVitals(FKodEntityId Id, int32 MaxHp, int32 Armor, int32 RegenMilliPerSec);
	/** Applies balance HP, damage, and regen after the spawn bridge registers the actor. */
	void AdoptScriptedSpawn(FKodEntityId Id, const FKodPendingUnitSpawn& Request);

	/** Broadcasts a floater. Nothing is stored on the sim, hashed, or saved. */
	void PushJuiceText(const FVector& Position, int32 Amount, uint8 Kind);

	/** HUD binds this. Cleared in Deinitialize. Not hashed and not saved. */
	FKodJuiceTextEvent OnJuiceText;
	EKodMatchOutcome GetMatchOutcome() const { return MatchOutcome; }
	bool IsMatchOver() const { return MatchOutcome != EKodMatchOutcome::None; }

	void SetSimPaused(bool bPaused) { bSimPaused = bPaused; }
	bool IsSimPaused() const { return bSimPaused; }
	void SetMenuMode(bool bEnabled) { bMenuMode = bEnabled; if (bEnabled) bSimPaused = true; }
	bool IsMenuMode() const { return bMenuMode; }

	bool SaveSkirmishSlot(int32 Slot);
	bool LoadSkirmishSlot(int32 Slot);

	void SetExposedRule(bool bEnabled);
	void SetExposedTimeSeconds(int32 Seconds);
	bool IsExposedRuleEnabled() const { return bExposedRule; }
	int32 GetExposedTimeSeconds() const { return ExposedTicks / 16; }
	bool IsTeamExposed(int32 TeamId) const;

	struct FKodStatSample
	{
		int32 Tick = 0;
		int32 Collected = 0;
		int32 Trained = 0;
		int32 ArmyValue = 0;
		int32 Income = 0;
		int32 Killed = 0;
		int32 Built = 0;
		int32 Razed = 0;
	};

	void CopyStatSamples(int32 TeamId, TArray<FKodStatSample>& Out) const;
	void CopyOlderStatSamples(int32 TeamId, TArray<FKodStatSample>& Out) const;
	void GetTeamTotals(int32 TeamId, int32& Collected, int32& Trained, int32& Killed, int32& Built, int32& Razed) const;

	bool IsMatchCheated() const { return bMatchCheated; }
	/** Last gold line, for the alert feed. Presentation only. Not hashed. */
	FString GetCheatFeedbackLine() const { return LastCheatLine; }
	/** Results line, for example "Cheats: Invincible (disabled), Show Me The Jade x2". */
	FString GetCheatResultLine() const;
	bool IsCompetitive() const { return bCompetitive; }
	void SetCompetitive(bool bEnabled);

	EKodMatchMode GetMatchMode() const { return MatchMode; }
	void SetMatchMode(EKodMatchMode Mode);
	bool AreCheatsAllowed() const;

	/** Dev console alias. Non-shipping registration. Refused outside campaign and offline skirmish. */
	bool ApplyCheatLine(const FString& Line);

	/**
	 * Exact chat phrase, case-insensitive.
	 * Applied raises cheat_enabled and sets the cheated flag.
	 * Refused logs KodCheat Refused Mode= and is ordinary chat.
	 */
	EKodCheatSubmit SubmitCheatPhrase(const FString& Line);

	/** Rename one phrase. Effect stays the same. */
	bool SetCheatPhrase(EKodCheatEffect Effect, const FString& Phrase);

	void RegisterAbilitySpec(const FKodAbilitySpec& Spec);
	bool UseOrbitalAbility(FKodEntityId CcId, FName AbilityId, FVector Target, FKodEntityId DepotId);

	void SetGeneralPowers(bool bEnabled);
	bool AreGeneralPowersEnabled() const { return bGeneralPowers; }
	int32 GetPowerPoints(int32 TeamId) const;
	int32 GetPowerCooldown(int32 TeamId, FName PowerId) const;
	bool TryGetPowerSpec(FName PowerId, FKodPowerSpec& OutSpec) const;
	bool CastPower(int32 TeamId, FName PowerId, FVector Target);
	void CopyArtilleryTelegraphs(int32 TeamId, TArray<int32>& OutX, TArray<int32>& OutY, TArray<int32>& OutRadius) const;

	/** Active recon sweeps. TicksLeft counts down from Duration. Presentation only. */
	void CopyReconPulses(int32 TeamId, TArray<int32>& OutX, TArray<int32>& OutY, TArray<int32>& OutRadius, TArray<int32>& OutTicksLeft, TArray<int32>& OutDuration) const;

	bool IssueLiftoff(FKodEntityId StructureId);
	bool IssueLand(FKodEntityId StructureId, FVector Target);
	void FinishOrbitalSpawn(FKodEntityId Id, const FKodPendingUnitSpawn& Request);
	bool TryGetOwnedIsr(int32 TeamId, FVector& OutCenter, int32& OutSight, int32& OutId) const;

	void AttachRestoredActor(FKodEntityId Id, AActor* Actor);
	void RestoreImportedState(FKodEntityId Id, const FKodSimEntityState& State);

	void EnsureBalanceTables();
	void ApplyBalance();
	bool SetBalanceField(FName Table, FName Row, FName Field, int32 Value);

	/** Headless seeded matchup. Stashes the live sim and restores it when the runs finish. */
	bool RunScriptedTest(FName Scenario, int32 Runs, int32 Seed);
	void LogTestSummary() const;

	/**
	 * Presentation snapshot of combat entities (not resource nodes).
	 * Does not mutate pose, orders, or the idle hash.
	 */
	void CopyEntitySnapshot(TArray<FKodSimEntityState>& OutStates, TArray<AActor*>& OutActors) const;

	/** Read-only copies for the canvas. Sorted by id. Does not mutate sim state. */
	void CopyResourceNodes(TArray<FKodResourceNodeState>& OutNodes) const;
	void CopyStructures(TArray<FKodStructureState>& OutStructures) const;

	/** Fired from the sim step when a hitscan shot is resolved. Tick is SimTickIndex at the shot. */
	UPROPERTY()
	FKodOnUnitFired OnUnitFired;

	/** Fired after damage is applied. Health is the remaining HP. */
	UPROPERTY()
	FKodOnUnitHit OnUnitHit;

	/** Fired after the target is unregistered. The actor is still alive for death presentation. */
	UPROPERTY()
	FKodOnUnitKilled OnUnitKilled;

	/** Fired when a train slot finishes inside the sim step. Location is the exit point. */
	UPROPERTY()
	FKodOnTrainCompleted OnTrainCompleted;

	/** Presentation only. Listeners play audio and must not write sim state. */
	UPROPERTY()
	FKodOnCue OnCue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Sim")
	int32 SimHz = KodBuildTicks::SimHz;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Sim")
	int32 MaxCatchUpSteps = KodBuildTicks::MaxCatchUpSteps;

	/** Position quantize for idle hash / snap (UU). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Sim")
	float PoseQuantizeUU = 1.f;

protected:
	void StepSim(float FixedDt);
	void StepEntityMove(FKodSimEntityState& State, float FixedDt);
	void StepEntityAttack(FKodSimEntityState& State, float FixedDt);
	/** Melee ring slot for this attacker. Derived from the acquire grid. Not stored. */
	FVector MeleeSlotPoint(const FKodSimEntityState& State, const FKodSimEntityState& Target);
	void StepAttackMove(FKodSimEntityState& State, float FixedDt);
	void StepHold(FKodSimEntityState& State, float FixedDt);
	void StepPatrol(FKodSimEntityState& State, float FixedDt);
	void StepFollow(FKodSimEntityState& State, float FixedDt, bool bGuard);
	void StepSeparation();
	bool SnapOutOfFootprint(FKodSimEntityState& State) const;
	void StepVision();
	void EnsureVisionStorage();
	int32 VisionCellIndex(int32 CellX, int32 CellY) const;
	bool TryGetLastSeenPoint(int32 TeamId, int32 EntityId, FVector& OutPoint) const;
	bool TryGetEnemyStart(FVector& OutPoint) const;
	void CompleteCurrentOrder(FKodSimEntityState& State, float FixedDt);
	bool TryApplyWeapon(FKodSimEntityState& State, FKodSimEntityState& Target);
	/** OnUnitKilled, unit_death, structure destroy, bunker cargo, and the kill log. */
	void FinishEntityDeath(FKodSimEntityState& Target, int32 KillerTeam);
	void EnsureCheatPhrases();
	bool FindCheatPhrase(const FString& Line, EKodCheatEffect& OutEffect) const;
	void ApplyCheatEffect(EKodCheatEffect Effect);
	void NoteCheatUse(EKodCheatEffect Effect, bool bEnabled, const FString& Detail);
	void GrantAllUpgrades(int32 TeamId);
	void KillEnemyUnitsOnScreen();
	void StripDeadFromCargo(int32 DeadId);
	void EnsureStructureDropOff(const FKodStructureState& Structure, int32 StandOverride);
	FVector GetSteeredGoal(FKodSimEntityState& State);
	void StepEntityGather(FKodSimEntityState& State, float FixedDt);
	/** Move toward Target and stop at AcceptUU. True when the worker is inside that radius. */
	bool AdvanceGatherMove(FKodSimEntityState& State, const FVector& Target, float AcceptUU, float FixedDt);
	void ClearGatherFields(FKodSimEntityState& State);
	void RefundGatherCargo(FKodSimEntityState& State);
	void AbortGather(FKodSimEntityState& State);
	bool RetargetGatherNode(FKodSimEntityState& State);
	void BeginGatherLeg(FKodSimEntityState& State, EKodGatherPhase Phase);
	/** True when a stall is close enough to count as arrival. False when the leg retargeted or the order aborted. */
	bool RecoverGatherStall(FKodSimEntityState& State, float Dist, float AcceptUU);
	FKodNodeTakeResult TakeFromNode(FKodEntityId Id, int32 Requested);
	/**
	 * Idle units with a weapon acquire a target and run StepEntityAttack.
	 * Move orders are left alone. Returns true when an attack step ran.
	 */
	bool TryAutoAcquire(FKodSimEntityState& State, float FixedDt);
	FKodEntityId FindNearestEnemyInRange(const FKodSimEntityState& State) const;
	FKodEntityId FindNearestEnemyStructureInRange(const FKodSimEntityState& State) const;
	/** Uniform grid for acquire scans. Rebuilt once per sim tick from sorted entity ids. Not hashed. */
	void RebuildAcquireGrid();
	void CollectAcquireCandidates(float OriginX, float OriginY, float Range, TArray<int32>& OutIds) const;
	FString GetEntityActorName(int32 IdValue) const;
	void QuantizePose(FKodSimEntityState& State) const;
	void QuantizeResourcePosition(FVector& Position) const;
	void SyncActorPresentation(float Alpha) const;
	void DestroyResourceNodeActor(FKodEntityId Id);
	void StepProduction();
	void StepAbilities();
	void StepAI();
	void StepPlayerAutoplay();
	void PollSoak(float DeltaTime);
	void ChooseAiOpener();
	void RememberAiMatch();
	void StepTech();
	void StepFaultlineWave();
	void ResetFaultlineScript();
	void EnqueueFaultlineUnit(FName UnitId, int32 Slot, uint8 Role, int32 Health, int32 Damage, int32 RangeUU, int32 Speed, int32 Armor, int32 CooldownTicks, int32 RegenMilli, int32 HomeX, int32 HomeY, int32 LeashUU, int32 AggroUU, const FVector& Rally, bool bAttackMove, FName Tag);
	void DespawnFaultlineRole(uint8 Role);
	void StepOrbital();
	void StepPowers();
	void ApplyReconVision();
	void StepLiftoff();
	void AwardPowerPoints(int32 TeamId, int32 Amount);
	/** One-shot starting pool from the PowerIncome row. Hashed via the granted flag and the point totals. */
	void GrantStartingPowerIncome();
	void CollectTaggedRamps();
	void StepExposed();
	void ApplyExposedReveal();
	void RecordMatchSample();
	void CollapseStatWindow(TArray<FKodStatSample>& Samples, TArray<FKodStatSample>& Older);
	void NoteResources(int32 TeamId, int32 Amount);
	void NoteUnitTrained(int32 TeamId);
	void NoteUnitKilled(int32 TeamId);
	void NoteStructureBuilt(int32 TeamId);
	void NoteStructureRazed(int32 TeamId);
	int32 ArmyValueFor(int32 TeamId) const;
	TArray<uint8> ExportSimBlob() const;
	void ImportSimBlob(const TArray<uint8>& Blob);
	void ResyncActors() const;
	bool CollectSavedEntityIds(const TArray<uint8>& Blob, TSet<int32>& OutIds) const;
	void DestroyEntitiesNotIn(const TSet<int32>& Keep);
	void QueueMissingActors();
	void AppendBalanceHash(TArray<uint8>& Bytes) const;
	void ApplyRoster(FKodEntityId Id);
	void EjectBunker(FKodEntityId BunkerId);
	bool CellHasUnit(int32 CellX, int32 CellY) const;
	bool IsCellTerrainBlocked(int32 CellX, int32 CellY) const;
	void InvalidatePathsOverFootprint(int32 CellX, int32 CellY, int32 SizeX, int32 SizeY);
	void EvaluateMatch();
	float EffectiveMoveSpeed(const FKodSimEntityState& State) const;
	void TryStartProduction(FKodStructureState& Structure);
	void FinishActiveProduction(FKodStructureState& Structure);
	void RecalcSupplyCap(int32 TeamId);
	void AddSupplyUsed(int32 TeamId, int32 Delta);
	void ReleaseCountedSupply(FKodEntityId Id);
	void StepConstruction();
	void ClearBuildAssignment(FKodSimEntityState& State);
	bool IsWorldCellBlocked(const FVector& World) const;
	FVector SteerAroundBuildings(const FKodSimEntityState& State, const FVector& DesiredDelta) const;
	bool TryGetGatherApproach(int32 TeamId, const FKodResourceNodeState& Node, FVector& OutPoint) const;
	FKodEntityId FindCompletedRig(int32 TeamId, FKodEntityId VentId) const;
	void ReleaseStructureRecord(FKodEntityId Id);

	UPROPERTY()
	TMap<int32, TWeakObjectPtr<AActor>> Entities;

	UPROPERTY()
	TMap<int32, FKodSimEntityState> States;

	/** Per-team Jadeite / Luminene. Integers only. Included in ComputeIdleHash. */
	UPROPERTY()
	TMap<int32, FKodResourceCost> Banks;

	/** Gather sources. Not iterated as combat. Included in ComputeIdleHash. */
	UPROPERTY()
	TMap<int32, FKodResourceNodeState> ResourceNodes;

	/** Drop-off buildings. A structure drop-off also has combat HP. Included in ComputeIdleHash. */
	UPROPERTY()
	TMap<int32, FKodDropOffState> DropOffs;

	TMap<int32, FKodSupplyState> Supply;
	TMap<int32, FKodStructureState> Structures;
	TMap<FName, FKodTrainSpec> TrainSpecs;
	TMap<FName, FKodBuildSpec> BuildSpecs;
	TMap<FName, FKodAbilitySpec> AbilitySpecs;
	TMap<FName, FKodUpgradeSpec> UpgradeSpecs;
	TMap<int32, FKodTeamTech> TeamTech;
	TMap<int32, int32> TeamFacingYaw;
	TMap<FName, TArray<FName>> UnitAbilities;
	TArray<FKodPendingUnitSpawn> PendingSpawns;
	TArray<FKodPendingBuildingSpawn> PendingBuildingSpawns;
	bool bHasPlayable = false;
	int32 PlayMinX = 0;
	int32 PlayMinY = 0;
	int32 PlayMaxX = 0;
	int32 PlayMaxY = 0;

	/** Previous step poses for interpolation. */
	TMap<int32, FVector> PrevPositions;
	TMap<int32, float> PrevYaws;

	int32 NextId = 1;
	float TimeAccumulator = 0.f;
	int64 SimTickIndex = 0;

	bool bAiEnabled = true;
	/** Team 0 AI. Not hashed and not saved. */
	bool bPlayerAutoplay = false;
	bool bSoakParsed = false;
	int32 SoakSeconds = 0;
	float SoakElapsed = 0.f;
	float SoakMemoryMark = -30.f;
	int64 SoakHashTick = -1;
	FString SoakHash;
	int64 PlayerNextWaveTick = 0;
	int32 AiDifficulty = 1;
	bool bAiSeeded = false;
	bool bAiScoutSent = false;
	bool bHasEnemyStart = false;
	int32 EnemyStartX = 0;
	int32 EnemyStartY = 0;
	int32 AiWavesLaunched = 0;
	int64 AiNextWaveTick = 0;
	EKodMatchOutcome MatchOutcome = EKodMatchOutcome::None;
	bool bTeam0HadStructure = false;
	bool bTeam1HadStructure = false;
	bool bSimPaused = false;
	bool bMenuMode = false;
	bool bExposedRule = true;
	int32 ExposedTicks = 6 * 60 * 16;
	bool bTeam0Exposed = false;
	bool bTeam1Exposed = false;
	bool bTeam0ExposedWarned = false;
	bool bTeam1ExposedWarned = false;

	struct FKodTeamTotals
	{
		int32 Collected = 0;
		int32 Trained = 0;
		int32 Killed = 0;
		int32 Built = 0;
		int32 Razed = 0;
		int32 LastIncomeCollected = 0;
	};
	FKodTeamTotals Totals0;
	FKodTeamTotals Totals1;
	TArray<FKodStatSample> Samples0;
	TArray<FKodStatSample> Samples1;
	TArray<FKodStatSample> OlderSamples0;
	TArray<FKodStatSample> OlderSamples1;

	bool bMatchCheated = false;
	bool bCompetitive = false;
	EKodMatchMode MatchMode = EKodMatchMode::OfflineSkirmish;
	TArray<FKodCheatUse> CheatUses;
	/** Alert text for the cheat just applied. Not hashed and not saved. */
	FString LastCheatLine;
	bool bGeneralPowers = true;
	/** Starting PowerIncome has been added. Hashed and saved so load does not grant twice. */
	bool bPowerStartGranted = false;
	/** Recent team-0 attack alerts. Hashed. 20s and 1500 uu throttle. */
	struct FKodAttackAlert
	{
		int32 TeamId = 0;
		int32 X = 0;
		int32 Y = 0;
		int64 Tick = 0;
	};
	TArray<FKodAttackAlert> AttackAlerts;
	/** 0 Vanguard, 1 Scinder. Hashed and saved. */
	uint8 PlayerFaction = 0;
	bool bFaultlineTraining = false;
	bool bFaultlineWaveSent = false;
	/** Log-once guard. Not hashed and not saved. */
	bool bFaultlineWaveErrorLogged = false;
	int32 FaultlineOutpostCount = 0;
	int32 FaultlineWaveX = 0;
	int32 FaultlineWaveY = 0;
	int32 FaultlineWaveZ = 0;
	int32 FaultlineHomeX = 0;
	int32 FaultlineHomeY = 0;
	int32 FaultlineHomeZ = 0;
	/** 0 armed, 1 waiting out the delay, 2 wave in the field, 3 falling back, 4 finished. */
	uint8 FaultlineWavePhase = 0;
	int64 FaultlineWaveDueTick = 0;
	int64 FaultlineWaveStartTick = 0;
	int64 FaultlineRetreatTick = 0;
	int32 FaultlineWaveStartHp = 0;
	int32 FaultlineWaveExpected = 0;
	uint8 bFaultlineGuardsSent = 0;
	int32 FaultlineOutpostX = 0;
	int32 FaultlineOutpostY = 0;
	int32 FaultlineOutpostZ = 0;
	int32 FaultlineLaneX = 0;
	int32 FaultlineLaneY = 0;
	int32 FaultlineLaneZ = 0;
	uint8 bFaultlineHasLane = 0;
	int32 FaultlineBarracksX = 0;
	int32 FaultlineBarracksY = 0;
	int32 FaultlineBarracksZ = 0;
	int32 FaultlineDelayTicks = 0;
	int32 FaultlineRetreatTicks = 0;
	int32 FaultlineHpPermille = 1000;
	int32 FaultlineGuardFighters = 0;
	int32 FaultlineGuardBlades = 0;
	int32 FaultlineGuardWorkers = 0;
	int32 FaultlineWaveFighters = 0;
	int32 FaultlineWaveBlades = 0;
	int32 FaultlineFighterLeashUU = 0;
	int32 FaultlineFighterAggroUU = 0;
	int32 FaultlineBladeLeashUU = 0;
	int32 FaultlineBladeAggroUU = 0;
	int32 FaultlineFighterHp = 1;
	int32 FaultlineFighterDamage = 0;
	int32 FaultlineFighterRangeUU = 0;
	int32 FaultlineFighterSpeed = 0;
	int32 FaultlineFighterArmor = 0;
	int32 FaultlineFighterCooldownTicks = 1;
	int32 FaultlineFighterRegenMilli = 0;
	int32 FaultlineBladeHp = 1;
	int32 FaultlineBladeDamage = 0;
	int32 FaultlineBladeRangeUU = 0;
	int32 FaultlineBladeSpeed = 0;
	int32 FaultlineBladeArmor = 0;
	int32 FaultlineBladeCooldownTicks = 1;
	int32 FaultlineBladeRegenMilli = 0;
	int32 FaultlineWorkerHp = 1;
	int32 FaultlineWorkerSpeed = 0;
	int32 FaultlineWorkerRegenMilli = 0;
	struct FKodScriptedUnit
	{
		int32 Id = 0;
		uint8 Role = 0;
		int32 HomeX = 0;
		int32 HomeY = 0;
		int32 LeashUU = 0;
		int32 AggroUU = 0;
		int32 RegenMilliPerSec = 0;
		int32 RegenAcc = 0;
		uint8 bOrdered = 0;
		int32 LastHealth = 0;
	};
	TArray<FKodScriptedUnit> ScriptedUnits;
	int32 PowerPoints0 = 0;
	int32 PowerPoints1 = 0;
	int32 ArtilleryCd0 = 0;
	int32 ArtilleryCd1 = 0;
	int32 ReconCd0 = 0;
	int32 ReconCd1 = 0;
	int32 RepairCd0 = 0;
	int32 RepairCd1 = 0;
	TArray<FKodScheduledPower> ScheduledPowers;
	TArray<int64> RampCells;
	/** Sorted unique high-ground cells. Vision only. Not saved. */
	TArray<int64> HighGroundCells;
	/** Sorted unique cliff cells. Hashed. */
	TArray<int64> DressBlockCells;
	/** Sorted unique LOS cells. Hashed. Fog only. */
	TArray<int64> VisionBlockCells;
	struct FKodDestructFoot
	{
		int32 Id = 0;
		int32 MinX = 0;
		int32 MinY = 0;
		int32 MaxX = 0;
		int32 MaxY = 0;
	};
	TArray<FKodDestructFoot> DestructFoots;
	TArray<FVector2D> LayoutSockets;
	TMap<FName, FKodPowerSpec> PowerSpecs;
	bool bCheatInvincible = false;
	bool bCheatInstantBuild = false;
	bool bCheatNoSupply = false;
	bool bCheatReveal = false;
	bool bCheatNoCooldown = false;
	int32 DamageDealt0 = 0;
	int32 DamageDealt1 = 0;
	int32 DamageTaken0 = 0;
	int32 DamageTaken1 = 0;
	int32 AiOpener = 0;
	bool bAiOpenerChosen = false;
	bool bScoutSawEnemy = false;
	bool bAiRetreating = false;
	bool bAiMemoryWritten = false;

	UPROPERTY()
	TObjectPtr<UDataTable> CheatTable;

	UPROPERTY()
	TObjectPtr<UDataTable> AbilityTable;
	UPROPERTY()
	TObjectPtr<UDataTable> UnitTable;
	UPROPERTY()
	TObjectPtr<UDataTable> WeaponTable;
	UPROPERTY()
	TObjectPtr<UDataTable> StructureTable;
	UPROPERTY()
	TObjectPtr<UDataTable> UpgradeTable;
	UPROPERTY()
	TObjectPtr<UDataTable> PowerTable;

	struct FKodTestResult
	{
		FString Scenario;
		int32 Seed = 0;
		int32 Run = 0;
		int32 Winner = -1;
		int32 TimeSeconds = 0;
		int32 Survivors0 = 0;
		int32 Survivors1 = 0;
		int32 Dealt0 = 0;
		int32 Dealt1 = 0;
		int32 Taken0 = 0;
		int32 Taken1 = 0;
		int32 Dps0 = 0;
		int32 Dps1 = 0;
		FString Hash;
	};
	TArray<FKodTestResult> TestResults;

	struct FKodMatchSnapshot
	{
		TArray<uint8> Blob;
		TMap<int32, TWeakObjectPtr<AActor>> Entities;
		TArray<FKodPendingUnitSpawn> PendingSpawns;
		TArray<FKodPendingBuildingSpawn> PendingBuildingSpawns;
		TMap<int32, FVector> PrevPositions;
		TMap<int32, float> PrevYaws;
		int32 NextId = 1;
		float TimeAccumulator = 0.f;
		bool bSimPaused = false;
		bool bMenuMode = false;
		bool bHasPlayable = false;
		int32 PlayMinX = 0;
		int32 PlayMinY = 0;
		int32 PlayMaxX = 0;
		int32 PlayMaxY = 0;
	};
	FKodMatchSnapshot TestStash;
	bool bTestIsolation = false;
	bool bLoadRespawn = false;
	int32 LoadRespawnTicks = 0;

	/** Cell -> sorted entity ids. Rebuilt at the start of each sim tick. */
	TMap<int64, TArray<int32>> AcquireGrid;
	/** Melee attackers by target id. Rebuilt from the acquire grid. Not hashed. */
	TMap<int32, TArray<int32>> MeleeAttackersByTarget;

	struct FKodStoredPath
	{
		int32 Id = 0;
		int64 LastUsedTick = 0;
		TArray<int32> CellX;
		TArray<int32> CellY;
	};

	struct FKodSeenStructure
	{
		int32 Id = 0;
		int32 TeamId = 0;
		FName DefinitionId;
		int32 PosX = 0;
		int32 PosY = 0;
		int32 FootprintX = 1;
		int32 FootprintY = 1;
		int32 Health = 0;
		int32 MaxHealth = 1;
	};

	struct FKodSeenUnit
	{
		int32 Id = 0;
		int32 TeamId = 0;
		int32 PosX = 0;
		int32 PosY = 0;
	};

	TArray<FKodStoredPath> StoredPaths;
	int32 NextPathId = 1;
	TArray<FKodStoredPath> TestStashPaths;
	int32 TestStashNextPathId = 1;
	bool bAiCheatVision = false;
	/** Playable build grid. Each team array is Width * Height, row-major. 0 unexplored, 1 explored, 2 visible. */
	int32 VisionOriginX = 0;
	int32 VisionOriginY = 0;
	int32 VisionWidth = 0;
	int32 VisionHeight = 0;
	TMap<int32, TArray<uint8>> TeamVision;
	TMap<int32, TArray<FKodSeenStructure>> SeenStructures;
	TMap<int32, TArray<FKodSeenUnit>> SeenUnits;
};
