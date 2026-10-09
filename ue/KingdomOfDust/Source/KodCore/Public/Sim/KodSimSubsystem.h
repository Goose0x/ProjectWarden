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
	Attack
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

	bool IsMoving() const { return Order == EKodSimOrderType::Move; }
	bool HasPendingOrder() const { return Order != EKodSimOrderType::None; }
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
		int32 TeamId = 0);

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
	 * Hashes entity ids, quantized positions, HP (DA-driven state), and order,
	 * then each team bank (Jadeite, Oil) and each resource node (id, type, remaining, quantized position).
	 * Those economy fields are integers. TeamId, RetaliateTarget, HarvestPerTrip, visual bob, visual yaw,
	 * and other presentation (flash, tracer, HP bar, death sink, resource readout) are not hashed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	FString ComputeIdleHash() const;

	/**
	 * Replace one team's bank. Match start only — gameplay uses Spend and Deposit.
	 * Team 0 is seeded in Initialize from StartingJadeite / StartingOil (50 / 0).
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void SetTeamBank(int32 TeamId, int32 Jadeite, int32 Oil);

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
	 * Logs KodEcon Deposit Team=.. Jadeite=.. Oil=.. Bank=J..,O..
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void Deposit(int32 TeamId, FKodResourceCost Amount);

	/**
	 * Register a placed or spawned node. Shares the entity id space with units but is not a combat state,
	 * so auto-acquire, hitscan, and HP bars ignore it. Amount <= 0 is rejected.
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	FKodEntityId RegisterResourceNode(
		AActor* Actor,
		EKodResourceType Type,
		int32 Amount,
		int32 HarvestPerTrip,
		FName DefinitionId);

	/** Drop the sim record. Does not destroy the actor (EndPlay uses this). */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	void UnregisterResourceNode(FKodEntityId Id);

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	bool IsResourceNode(FKodEntityId Id) const;

	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	bool TryGetResourceNode(FKodEntityId Id, FKodResourceNodeState& OutNode) const;

	/** 2D nearest node. Ties go to the lowest entity id. Origin is not hashed. */
	UFUNCTION(BlueprintPure, Category = "Kod|Economy")
	FKodEntityId FindNearestResourceNode(FVector Origin) const;

	/**
	 * Take up to Amount from the node and Deposit it into TeamId.
	 * Amount is clamped to Remaining. Remaining 0 unregisters and destroys the actor.
	 * Logs the Deposit line, then KodEcon Node Id=.. Type=.. Remaining=..
	 * Returns the amount taken (0 on a miss).
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Economy")
	int32 HarvestNode(FKodEntityId Id, int32 Amount, int32 TeamId);

	/** Default 50. L_Slice0 overwrites this from AKodSlice0GameMode at StartPlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 StartingJadeite = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Economy")
	int32 StartingOil = 0;

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

	/** Per-team Jadeite / Oil. Integers only. Included in ComputeIdleHash. */
	UPROPERTY()
	TMap<int32, FKodResourceCost> Banks;

	/** Gather sources. Not iterated by StepSim. Included in ComputeIdleHash. */
	UPROPERTY()
	TMap<int32, FKodResourceNodeState> ResourceNodes;

	/** Previous step poses for interpolation. */
	TMap<int32, FVector> PrevPositions;
	TMap<int32, float> PrevYaws;

	int32 NextId = 1;
	float TimeAccumulator = 0.f;
	int64 SimTickIndex = 0;
};
