#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequenceBase.h"

class AKodUnit;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKodUnitFireSignature);

#include "KodUnitAnimInstance.generated.h"

/**
 * Parent for the Marine AnimBP. Presentation only: reads sim speed and orders,
 * never writes pose, orders, or the idle hash.
 *
 * AnimBP: parent this class. Idle/Run blend on Speed. Aim when bIsAiming.
 * Fire when FireCounter changes (or bind OnFire), played at FirePlayRate.
 * Additive HitReact on bHitReact, Mesh Space, base AimIdle frame 0.
 * Death when bIsDead. bDeathVariantB picks Death_B (odd entity id) or Death (even).
 * The sink starts 0.2 s after that clip's hold frame, not at the clip end.
 */
UCLASS(Blueprintable, BlueprintType)
class KODUNITS_API UKodUnitAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Sim fire. Bumps FireCounter and broadcasts OnFire. */
	void HandleSimFired();

	/** One-frame pulse the AnimBP reads as an additive hit trigger. */
	void HandleSimHit();

	void HandleSimDeath();

	/**
	 * Seconds from death start until the sink. 0 when neither death clip is set.
	 * Otherwise holdFrame / AuthoredFrameRate + DeathSinkAfterHoldSeconds.
	 */
	float GetDeathSinkDelaySeconds() const;

	/** Horizontal speed in cm/s. 0 when standing, including aiming in place. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Anim")
	float Speed = 0.f;

	/** True while the sim order is Attack and the target is still alive. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Anim")
	bool bIsAiming = false;

	/** True once death presentation has started. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Anim")
	bool bIsDead = false;

	/**
	 * True for one anim update after a hit, then cleared.
	 * Use as the additive HitReact trigger. Base pose is AimIdle frame 0.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Anim")
	bool bHitReact = false;

	/** Increments on each sim shot so a transition can watch it. */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Anim")
	int32 FireCounter = 0;

	/** Broadcast with FireCounter. AnimBP can bind this as well as the counter. */
	UPROPERTY(BlueprintAssignable, Category = "Kod|Anim")
	FKodUnitFireSignature OnFire;

	/**
	 * Authored run speed, cm/s. Stride is 300 cm/cycle.
	 * RunPlayRate = Speed / AuthoredRunSpeed.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	float AuthoredRunSpeed = 450.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Anim")
	float RunPlayRate = 1.f;

	/** Authored walk speed, cm/s. WalkPlayRate = Speed / AuthoredWalkSpeed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	float AuthoredWalkSpeed = 150.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Anim")
	float WalkPlayRate = 1.f;

	/**
	 * Fire state play rate. 11 frames at 30 fps is (11-1)/30 s;
	 * 0.952 stretches that to 0.35 s. Shot notify is on frame 1.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	float FirePlayRate = 0.952f;

	/** Authored clip rate used to turn death hold frames into seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	float AuthoredFrameRate = 30.f;

	/** Seconds after the death hold frame before the body starts sinking. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	float DeathSinkAfterHoldSeconds = 0.2f;

	/** Death clip. Even entity ids. Holds from DeathHoldFrame. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	TObjectPtr<UAnimSequenceBase> DeathAnim;

	/** Death_B clip. Odd entity ids. Holds from DeathHoldFrameB. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	TObjectPtr<UAnimSequenceBase> DeathAnimB;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	int32 DeathHoldFrame = 26;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	int32 DeathHoldFrameB = 30;

	/** True when this unit plays Death_B (odd entity id). */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Anim")
	bool bDeathVariantB = false;

private:
	bool bHitReactClearNextUpdate = false;

	void ReadSimPresentation(AKodUnit* UnitPawn);
};
