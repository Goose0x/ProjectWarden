#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "Sim/KodEntityId.h"
#include "KodUnit.generated.h"

class UKodUnitDefinition;
class UKodAbilitySystemComponent;
class UKodCombatAttributeSet;
class UKodMoveComponent;
class UKodAttackComponent;
class USkeletalMesh;
class UStaticMesh;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UKodSimSubsystem;

/**
 * Selectable mobile combatant. Definition soft-ref drives stats at BeginPlay.
 * Body mesh is presentation: definition static mesh when it loads, otherwise the Engine cube.
 */
UCLASS(Blueprintable)
class KODUNITS_API AKodUnit : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	AKodUnit();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kod|Data")
	TSoftObjectPtr<UKodUnitDefinition> Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId EntityId;

	UPROPERTY(BlueprintReadWrite, Category = "Kod|Team")
	int32 TeamId = 0;

	/**
	 * Vertical bob amplitude on UnitMesh, in cm, before the 170 cm body-height scale.
	 * Presentation only. Does not write sim pose or the idle hash.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Presentation")
	float BobAmplitudeCm = 3.5f;

	/** Walking cadence for the bob sine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Presentation")
	float BobFrequencyHz = 2.5f;

	/** How fast the bob eases in while moving and back to rest when stopped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Presentation")
	float BobEaseSpeed = 8.f;

	/**
	 * Visual body-yaw catch-up in degrees per second (RInterpConstantTo).
	 * Applied as a UnitMesh relative yaw so it still shows if this tick runs
	 * before or after the sim sync. Faces sim movement, and the attack target
	 * while attacking. Does not write FKodSimEntityState::YawDegrees.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Presentation")
	float TurnRateDegreesPerSecond = 540.f;

	UFUNCTION(BlueprintCallable, Category = "Kod|Data")
	UKodUnitDefinition* GetDefinition() const;

	UFUNCTION(BlueprintCallable, Category = "Kod|Data")
	void ApplyDefinition(UKodUnitDefinition* Def);

	/** When set before ApplyDefinition, the visible body stays the Engine cube. */
	void SetForceCubeBody(bool bInForce);

	/**
	 * Hostile marker. Forces the Engine cube (no Ranger static mesh, no overlay material)
	 * and tints slot 0 with a MID of /Engine/BasicShapes/BasicShapeMaterial.
	 * Sets Color and BaseColor. BasicShapeMaterial honors Color. Presentation only.
	 */
	void ApplyHostileCubeTint();

protected:
	/** Skeletal mesh wins when it loads; else static mesh on UnitMesh; else the Engine cube. */
	void ApplyBodyMesh(const UKodUnitDefinition* Def);
	void MountResolvedSkeletalMesh(USkeletalMesh* SkelMesh);
	void MountResolvedStaticMesh(UStaticMesh* StaticBody);
	void MountCubePlaceholder();

	/** QueryOnly Pawn on UnitMesh. Same contract as the constructor — does not touch the capsule. */
	void KeepSelectCollision();

	/** Bob + visual yaw. Runs after sim presentation sync. Never writes sim state. */
	void UpdateLifePresentation(float DeltaSeconds);
	float GetScaledBobAmplitudeCm() const;

	void BindSimEvents(UKodSimSubsystem* Sim);
	void UnbindSimEvents();

	UFUNCTION()
	void HandleUnitFired(AActor* Attacker, AActor* Target, int32 SimTick);

	UFUNCTION()
	void HandleUnitHit(AActor* Attacker, AActor* Target, float Health, int32 TargetId);

	UFUNCTION()
	void HandleUnitKilled(AActor* Target, int32 TargetId);

	/** Muzzle light, flash mesh, and tracer. Presentation only. Hidden until a shot. */
	void EnsureAttackPresentation();
	void ShowMuzzleAndTracer(AActor* Target);
	void AdvanceAttackPresentation(float DeltaSeconds);
	void EnsureBodyTint();
	void SetBodyRelativeLocation(const FVector& Bobbed);
	void BeginDeathPresentation();
	/** Returns true when the actor was destroyed. */
	bool AdvanceDeathSink(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|GAS")
	TObjectPtr<UKodAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UKodCombatAttributeSet> CombatAttributes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Components")
	TObjectPtr<UKodMoveComponent> MoveComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Components")
	TObjectPtr<UKodAttackComponent> AttackComponent;

	/**
	 * Static mesh child. Slice 0 swaps in UKodUnitDefinition::StaticMesh when that soft path loads.
	 * Constructor plants /Engine/BasicShapes/Cube at 0.8×0.8×1.7 when nothing resolves.
	 * A resolved static body is uniformly scaled from its bounds to ~170 cm tall (Slice 0
	 * band-aid for meter-imported meshes). The cube path does not use that scale.
	 * Collision stays QueryOnly Pawn (Visibility ignored via the Pawn profile). Capsule is not retuned.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Mesh")
	TObjectPtr<UStaticMeshComponent> UnitMesh;

	/** Slice 0 hostile path. ApplyBodyMesh skips the definition static mesh. */
	bool bForceCubeBody = false;

	/** Captured attach point. Bob is an offset on top of this, not a new scale. */
	FVector BodyRestRelativeLocation = FVector::ZeroVector;

	float VisualYaw = 0.f;
	float BobWeight = 0.f;
	float BobTime = 0.f;
	FVector LastPresentationLocation = FVector::ZeroVector;
	bool bBobOffsetApplied = false;

	TWeakObjectPtr<UKodSimSubsystem> BoundSim;

	/** Slot 0 tint. Hostile red is stored so a hit flash can restore it. Presentation only. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyTintMID;

	FLinearColor BodyRestColor = FLinearColor::White;
	float HitReactRemaining = 0.f;
	FVector HitJiggleLocal = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> MuzzleFlashLight;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MuzzleFlashMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> MuzzleConeMesh;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> ShotTracerMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MuzzleMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> TracerMID;

	float MuzzleFlashRemaining = 0.f;
	float TracerRemaining = 0.f;

	bool bDeathSinking = false;
	float DeathSinkElapsed = 0.f;
	FVector DeathSinkStart = FVector::ZeroVector;
};
