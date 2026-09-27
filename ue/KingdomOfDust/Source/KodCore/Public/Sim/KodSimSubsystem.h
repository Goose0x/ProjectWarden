#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Sim/KodEntityId.h"
#include "Sim/KodBuildTicks.h"
#include "KodSimSubsystem.generated.h"

class AActor;

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
		float WeaponCooldownSeconds);

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
	 * Hashes entity ids, quantized positions, HP (DA-driven state).
	 */
	UFUNCTION(BlueprintCallable, Category = "Kod|Sim")
	FString ComputeIdleHash() const;

	UFUNCTION(BlueprintPure, Category = "Kod|Sim")
	int64 GetSimTickIndex() const { return SimTickIndex; }

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
	void QuantizePose(FKodSimEntityState& State) const;
	void SyncActorPresentation(float Alpha) const;

	UPROPERTY()
	TMap<int32, TWeakObjectPtr<AActor>> Entities;

	UPROPERTY()
	TMap<int32, FKodSimEntityState> States;

	/** Previous step poses for interpolation. */
	TMap<int32, FVector> PrevPositions;
	TMap<int32, float> PrevYaws;

	int32 NextId = 1;
	float TimeAccumulator = 0.f;
	int64 SimTickIndex = 0;
};
