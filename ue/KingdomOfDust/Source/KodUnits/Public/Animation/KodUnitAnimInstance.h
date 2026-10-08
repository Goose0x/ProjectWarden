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
 * Fire when FireCounter changes (or bind OnFire). Additive HitReact on bHitReact.
 * Death when bIsDead, hold the last frame. Set DeathAnim to that clip so the
 * actor waits until ~0.2 s after it ends before the sink.
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
	 * Clip length used as the authored run, in cm/s.
	 * RunPlayRate = Speed / AuthoredRunSpeed. Ranger move speed is 450.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	float AuthoredRunSpeed = 450.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Anim")
	float RunPlayRate = 1.f;

	/**
	 * Death clip (sequence or montage) the AnimBP plays when bIsDead.
	 * Empty means the actor sinks immediately. A set clip delays the sink
	 * until GetPlayLength + 0.2 s.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Anim")
	TObjectPtr<UAnimSequenceBase> DeathAnim;

private:
	bool bHitReactClearNextUpdate = false;

	void ReadSimPresentation(AKodUnit* UnitPawn);
};
