#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Sim/KodEntityId.h"
#include "Sim/KodBuildTicks.h"
#include "Sim/KodResourceTypes.h"
#include "KodSimSubsystem.generated.h"

class AActor;

/** Presentation binds these. Listeners must not write sim state or the idle hash. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FKodOnUnitFired, AActor*, Attacker, AActor*, Target, int32, SimTick);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FKodOnUnitHit, AActor*, Attacker, AActor*, Target, float, Health, int32, TargetId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FKodOnUnitKilled, AActor*, Target, int32, TargetId);

UENUM(BlueprintType)
enum class EKodSimOrderType : uint8
{
	None,
	Move,
	Attack,
	Gather
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
	 * Attacker to retaliate against after a hit taken while Idle.
	 * Not an order. Consumed by auto-acquire. Not part of ComputeIdleHash.
	 * An explicit Move / Attack / Stop clears it.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId RetaliateTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float WeaponDamage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float WeaponRange = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	float WeaponCooldownSeconds = 0.f;

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

	bool IsMoving() const { return Order == EKodSimOrderType::Move; }
	bool HasPendingOrder() const { return Order != EKodSimOrderType::None; }
	bool IsCarryingCargo() const { return CargoJadeite > 0 || CargoLuminene > 0; }
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
		bool bCanGather = false);

	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	void IssueMove(FKodEntityId Id, FVector WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	void IssueAttack(FKodEntityId Id, FKodEntityId TargetId);

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
	 * cargo, harvest ticks, stall), then each team bank (Jadeite, Luminene), each resource node
	 * (id, type, remaining, quantized position), and each drop-off (id, team, quantized position).
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

	/** Default 50. L_Slice0 overwrites this from AKodSlice0GameMode at StartPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 StartingJadeite = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 StartingLuminene = 0;

	UFUNCTION(BlueprintPure, Category = "Kod|Sim")
	int64 GetSimTickIndex() const { return SimTickIndex; }

	/**
	 * Presentation snapshot of combat entities (not resource nodes).
	 * Does not mutate pose, orders, or the idle hash.
	 */
	void CopyEntitySnapshot(TArray<FKodSimEntityState>& OutStates, TArray<AActor*>& OutActors) const;

	/** Fired from the sim step when a hitscan shot is resolved. Tick is SimTickIndex at the shot. */
	UPROPERTY()
	FKodOnUnitFired OnUnitFired;

	/** Fired after damage is applied. Health is the remaining HP. */
	UPROPERTY()
	FKodOnUnitHit OnUnitHit;

	/** Fired after the target is unregistered. The actor is still alive for death presentation. */
	UPROPERTY()
	FKodOnUnitKilled OnUnitKilled;

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
	FString GetEntityActorName(int32 IdValue) const;
	void QuantizePose(FKodSimEntityState& State) const;
	void QuantizeResourcePosition(FVector& Position) const;
	void SyncActorPresentation(float Alpha) const;
	void DestroyResourceNodeActor(FKodEntityId Id);

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

	/** Drop-off buildings. Not combat. Included in ComputeIdleHash. */
	UPROPERTY()
	TMap<int32, FKodDropOffState> DropOffs;

	/** Previous step poses for interpolation. */
	TMap<int32, FVector> PrevPositions;
	TMap<int32, float> PrevYaws;

	int32 NextId = 1;
	float TimeAccumulator = 0.f;
	int64 SimTickIndex = 0;
};
