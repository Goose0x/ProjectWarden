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
class UMeshComponent;
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

	/** USA sand #D2C4B1. TeamColor on large armour. Palette is pending Art. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Team")
	FLinearColor FriendlyTeamColor;

	/** Hostile red #B3261E. Same TeamColor param. Editable per unit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Team")
	FLinearColor HostileTeamColor;

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
	 * Skeletal: relative yaw is SkeletalMeshYawOffset plus this, so the mesh
	 * turns even when actor yaw already snapped. Cube/static: UnitMesh relative yaw.
	 * Faces travel on a Move, and the target while attacking. Does not write sim yaw.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Presentation")
	float TurnRateDegreesPerSecond = 900.f;

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

	/**
	 * Team tint after the body is mounted. Sets TeamColor on a dynamic MID when the
	 * material has that vector param (TeamMask lives on the material). Otherwise team 1
	 * uses the Color/BaseColor tint. Team 0 with no TeamColor param is left untinted.
	 * Does not write sim state.
	 */
	void ApplyTeamColor();

	/** Skeletal, or Cube when the skeletal soft ref did not load. StaticMesh if that body won. */
	const TCHAR* GetMountedBodyName() const;

	/** True after death presentation starts, including a skeletal clip hold before the sink. */
	bool IsDeathSinking() const { return bDeathSinking; }

	/**
	 * Shot notify entry. Uses SOCKET_Muzzle, then weapon_r.
	 * Missing sockets use the timed body-front flash.
	 */
	void PlayMuzzleFromShotNotify();

	/** Called when the anim instance starts so native clips and death frames load. */
	void InitializeAnimFromDefinition(class UKodUnitAnimInstance* Anim);

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

	/** Small mesh while the sim says this worker is carrying. Presentation only. */
	void UpdateCargoPresentation();
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
	void ShowMuzzleAndTracer(AActor* Target, bool bUseMuzzleWorld, FVector MuzzleWorld);
	bool TryResolveMuzzleSocket(FVector& OutWorld) const;
	void PushAnimSimEvent(bool bFired, bool bHit, bool bDead);
	float GetSkeletalDeathHoldSeconds() const;
	void ApplySkeletalCapsule();
	void RestoreDefaultCapsule();
	void AttachWeaponMesh(USkeletalMeshComponent* SkelBody, const UKodUnitDefinition* BodyDef);
	void ClearWeaponMesh();
	void LogAnimMode(const TCHAR* ModeName) const;
	bool DefinitionHasAnimSequence(const UKodUnitDefinition* BodyDef) const;
	void AdvanceAttackPresentation(float DeltaSeconds);
	void EnsureBodyTint();
	void WriteBodyTint(const FLinearColor& Tint);
	void ApplyColorTintFallback(UMeshComponent* VisualBody, const FLinearColor& Tint);
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

	/** True only while the Character skeletal mesh is the visible body. */
	bool bSkeletalBody = false;

	/** Capsule was resized for the skeletal body and should be restored on the cube path. */
	bool bSkeletalCapsule = false;

	/** AnimBP is in use; wait briefly for UKodAnimNotify_Shot before the timed flash. */
	bool bAwaitShotNotify = false;
	float ShotNotifyWait = 0.f;
	TWeakObjectPtr<AActor> PendingMuzzleTarget;

	/** Seconds to hold before the 1.5 s sink. 0 keeps the immediate sink. */
	float DeathSinkDelayRemaining = 0.f;

	/** Captured attach point. Bob is an offset on top of this, not a new scale. */
	FVector BodyRestRelativeLocation = FVector::ZeroVector;

	float MountedMeshYawOffset = -90.f;
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
	bool bTeamColorApplied = false;
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

	/** Carbine on weapon_r. Persistent, so it is not tagged KodFx and takes the selection rim. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> WeaponMeshComp;

	/** Carried Jadeite or Luminene. Tagged KodFx so the selection rim skips it. */
	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> CargoMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CargoMID;

	int32 ShownCargoJadeite = -1;
	int32 ShownCargoLuminene = -1;

	float MuzzleFlashRemaining = 0.f;
	float TracerRemaining = 0.f;

	bool bDeathSinking = false;
	float DeathSinkElapsed = 0.f;
	FVector DeathSinkStart = FVector::ZeroVector;
};
