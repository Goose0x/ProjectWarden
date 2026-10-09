#include "Animation/KodUnitAnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
#include "Animation/KodAnimNotify_Shot.h"
#include "Actors/KodUnit.h"
#include "Data/KodUnitDefinition.h"
#include "Sim/KodSimSubsystem.h"

namespace
{
	constexpr float NativeBlendSeconds = 0.15f;
	constexpr float AimStationarySpeed = 40.f;
	constexpr float ShotFrameSeconds = 1.f / 30.f;

	struct FKodNativePoseSnapshot
	{
		UAnimSequence* Idle = nullptr;
		UAnimSequence* Walk = nullptr;
		UAnimSequence* Run = nullptr;
		UAnimSequence* Aim = nullptr;
		UAnimSequence* Fire = nullptr;
		UAnimSequence* Hit = nullptr;
		UAnimSequence* Death = nullptr;
		bool bNative = false;
		bool bHitActive = false;
		float IdleTime = 0.f;
		float WalkTime = 0.f;
		float RunTime = 0.f;
		float AimTime = 0.f;
		float FireTime = 0.f;
		float HitTime = 0.f;
		float DeathTime = 0.f;
		float AimBlend = 0.f;
		float FireBlend = 0.f;
		float DeathBlend = 0.f;
		float IdleWeight = 0.f;
		float WalkWeight = 0.f;
		float RunWeight = 0.f;
	};

	void SampleSequence(UAnimSequence* Sequence, float Time, const FBoneContainer& Bones, FCompactPose& OutPose, FBlendedCurve& OutCurve, UE::Anim::FStackAttributeContainer& OutAttrs)
	{
		OutPose.SetBoneContainer(&Bones);
		OutCurve.InitFrom(Bones);
		OutAttrs = UE::Anim::FStackAttributeContainer();
		if (!Sequence)
		{
			OutPose.ResetToRefPose();
			return;
		}
		FAnimationPoseData PoseData(OutPose, OutCurve, OutAttrs);
		FAnimExtractContext Context;
		Context.CurrentTime = Time;
		Context.bExtractRootMotion = false;
		Sequence->GetAnimationPose(PoseData, Context);
	}

	void BlendOnto(FCompactPose& BasePose, FBlendedCurve& BaseCurve, UE::Anim::FStackAttributeContainer& BaseAttrs, FCompactPose& AddPose, FBlendedCurve& AddCurve, UE::Anim::FStackAttributeContainer& AddAttrs, float AddWeight)
	{
		const float Clamped = FMath::Clamp(AddWeight, 0.f, 1.f);
		if (Clamped <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		FCompactPose ResultPose;
		FBlendedCurve ResultCurve;
		UE::Anim::FStackAttributeContainer ResultAttrs;
		ResultPose.SetBoneContainer(&BasePose.GetBoneContainer());
		ResultCurve.InitFrom(BasePose.GetBoneContainer());
		FAnimationPoseData BaseData(BasePose, BaseCurve, BaseAttrs);
		FAnimationPoseData AddData(AddPose, AddCurve, AddAttrs);
		FAnimationPoseData ResultData(ResultPose, ResultCurve, ResultAttrs);
		// WeightOfPoseOne is the first pose. Keep Base at (1 - AddWeight).
		FAnimationRuntime::BlendTwoPosesTogether(BaseData, AddData, 1.f - Clamped, ResultData);
		BasePose.CopyBonesFrom(ResultPose);
		BaseCurve.CopyFrom(ResultCurve);
	}

	void AccumulateHit(FCompactPose& BasePose, FBlendedCurve& BaseCurve, UE::Anim::FStackAttributeContainer& BaseAttrs, UAnimSequence* HitSeq, float Time, const FBoneContainer& Bones)
	{
		if (!HitSeq)
		{
			return;
		}
		FCompactPose HitPose;
		FBlendedCurve HitCurve;
		UE::Anim::FStackAttributeContainer HitAttrs;
		SampleSequence(HitSeq, Time, Bones, HitPose, HitCurve, HitAttrs);
		FAnimationPoseData BaseData(BasePose, BaseCurve, BaseAttrs);
		FAnimationPoseData HitData(HitPose, HitCurve, HitAttrs);
		const EAdditiveAnimationType AdditiveType = HitSeq->GetAdditiveAnimType();
		if (AdditiveType == AAT_RotationOffsetMeshSpace)
		{
			FAnimationRuntime::AccumulateMeshSpaceRotationAdditiveToLocalPose(BaseData, HitData, 1.f);
		}
		else if (AdditiveType == AAT_LocalSpaceBase)
		{
			FAnimationRuntime::AccumulateAdditivePose(BaseData, HitData, 1.f, AAT_LocalSpaceBase);
		}
		else
		{
			BlendOnto(BasePose, BaseCurve, BaseAttrs, HitPose, HitCurve, HitAttrs, 1.f);
		}
	}
}

struct FKodUnitAnimInstanceProxy : public FAnimInstanceProxy
{
	FKodUnitAnimInstanceProxy() = default;
	explicit FKodUnitAnimInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override
	{
		FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
		if (const UKodUnitAnimInstance* KodAnim = Cast<UKodUnitAnimInstance>(InAnimInstance))
		{
			NativeSnapshot.bNative = KodAnim->bNativePoseDriver;
			NativeSnapshot.Idle = KodAnim->NativeIdle;
			NativeSnapshot.Walk = KodAnim->NativeWalk;
			NativeSnapshot.Run = KodAnim->NativeRun;
			NativeSnapshot.Aim = KodAnim->NativeAim;
			NativeSnapshot.Fire = KodAnim->bFireActive ? KodAnim->NativeFire.Get() : nullptr;
			NativeSnapshot.Hit = KodAnim->NativeHit;
			NativeSnapshot.bHitActive = KodAnim->bHitActive && KodAnim->NativeHit != nullptr;
			const bool bPlayB = KodAnim->bDeathVariantB && KodAnim->NativeDeathB != nullptr;
			NativeSnapshot.Death = bPlayB ? KodAnim->NativeDeathB.Get() : KodAnim->NativeDeath.Get();
			if (!NativeSnapshot.Death)
			{
				NativeSnapshot.Death = bPlayB ? KodAnim->NativeDeath.Get() : KodAnim->NativeDeathB.Get();
			}
			NativeSnapshot.IdleTime = KodAnim->IdleTime;
			NativeSnapshot.WalkTime = KodAnim->WalkTime;
			NativeSnapshot.RunTime = KodAnim->RunTime;
			NativeSnapshot.AimTime = KodAnim->AimTime;
			NativeSnapshot.FireTime = KodAnim->FireTime;
			NativeSnapshot.HitTime = KodAnim->HitTime;
			NativeSnapshot.DeathTime = KodAnim->DeathTime;
			NativeSnapshot.AimBlend = KodAnim->AimBlend;
			NativeSnapshot.FireBlend = KodAnim->bFireActive ? KodAnim->FireBlend : 0.f;
			NativeSnapshot.DeathBlend = KodAnim->DeathBlend;

			const float MoveSpeed = KodAnim->Speed;
			const float WalkSpeed = KodAnim->AuthoredWalkSpeed;
			const float RunSpeed = KodAnim->AuthoredRunSpeed;
			NativeSnapshot.IdleWeight = 0.f;
			NativeSnapshot.WalkWeight = 0.f;
			NativeSnapshot.RunWeight = 0.f;
			if (MoveSpeed <= KINDA_SMALL_NUMBER)
			{
				NativeSnapshot.IdleWeight = 1.f;
			}
			else if (MoveSpeed < WalkSpeed || RunSpeed <= WalkSpeed)
			{
				const float Alpha = FMath::Clamp(MoveSpeed / FMath::Max(1.f, WalkSpeed), 0.f, 1.f);
				NativeSnapshot.IdleWeight = 1.f - Alpha;
				NativeSnapshot.WalkWeight = Alpha;
			}
			else if (MoveSpeed < RunSpeed)
			{
				const float Span = FMath::Max(1.f, RunSpeed - WalkSpeed);
				const float Alpha = FMath::Clamp((MoveSpeed - WalkSpeed) / Span, 0.f, 1.f);
				NativeSnapshot.WalkWeight = 1.f - Alpha;
				NativeSnapshot.RunWeight = Alpha;
			}
			else
			{
				NativeSnapshot.RunWeight = 1.f;
			}
		}
	}

	virtual bool Evaluate(FPoseContext& Output) override
	{
		if (!NativeSnapshot.bNative)
		{
			return FAnimInstanceProxy::Evaluate(Output);
		}

		const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
		if (!Bones.IsValid())
		{
			return false;
		}

		Output.Pose.ResetToRefPose();

		FCompactPose AccPose;
		FBlendedCurve AccCurve;
		UE::Anim::FStackAttributeContainer AccAttrs;
		AccPose.SetBoneContainer(&Bones);
		AccCurve.InitFrom(Bones);
		AccPose.ResetToRefPose();

		float IdleW = NativeSnapshot.Idle ? NativeSnapshot.IdleWeight : 0.f;
		float WalkW = NativeSnapshot.Walk ? NativeSnapshot.WalkWeight : 0.f;
		float RunW = NativeSnapshot.Run ? NativeSnapshot.RunWeight : 0.f;
		const float LocoSum = IdleW + WalkW + RunW;
		if (LocoSum > KINDA_SMALL_NUMBER)
		{
			IdleW /= LocoSum;
			WalkW /= LocoSum;
			RunW /= LocoSum;
		}

		bool bHavePose = false;
		float LocoAccum = 0.f;
		auto AddFull = [&](UAnimSequence* Sequence, float Time, float Portion)
		{
			if (!Sequence || Portion <= KINDA_SMALL_NUMBER)
			{
				return;
			}
			FCompactPose SamplePose;
			FBlendedCurve SampleCurve;
			UE::Anim::FStackAttributeContainer SampleAttrs;
			SampleSequence(Sequence, Time, Bones, SamplePose, SampleCurve, SampleAttrs);
			if (!bHavePose)
			{
				AccPose.CopyBonesFrom(SamplePose);
				AccCurve.CopyFrom(SampleCurve);
				bHavePose = true;
				return;
			}
			BlendOnto(AccPose, AccCurve, AccAttrs, SamplePose, SampleCurve, SampleAttrs, Portion);
		};

		auto AddLoco = [&](UAnimSequence* Sequence, float Time, float Weight)
		{
			if (!Sequence || Weight <= KINDA_SMALL_NUMBER)
			{
				return;
			}
			if (!bHavePose)
			{
				AddFull(Sequence, Time, 1.f);
				LocoAccum = Weight;
				return;
			}
			const float NewTotal = LocoAccum + Weight;
			const float Portion = NewTotal > KINDA_SMALL_NUMBER ? (Weight / NewTotal) : 0.f;
			AddFull(Sequence, Time, Portion);
			LocoAccum = NewTotal;
		};

		AddLoco(NativeSnapshot.Idle, NativeSnapshot.IdleTime, IdleW);
		AddLoco(NativeSnapshot.Walk, NativeSnapshot.WalkTime, WalkW);
		AddLoco(NativeSnapshot.Run, NativeSnapshot.RunTime, RunW);
		if (NativeSnapshot.Aim && NativeSnapshot.AimBlend > KINDA_SMALL_NUMBER)
		{
			AddFull(NativeSnapshot.Aim, NativeSnapshot.AimTime, NativeSnapshot.AimBlend);
		}
		if (NativeSnapshot.Fire && NativeSnapshot.FireBlend > KINDA_SMALL_NUMBER)
		{
			AddFull(NativeSnapshot.Fire, NativeSnapshot.FireTime, NativeSnapshot.FireBlend);
		}
		if (NativeSnapshot.Death && NativeSnapshot.DeathBlend > KINDA_SMALL_NUMBER)
		{
			AddFull(NativeSnapshot.Death, NativeSnapshot.DeathTime, NativeSnapshot.DeathBlend);
		}
		if (NativeSnapshot.bHitActive)
		{
			AccumulateHit(AccPose, AccCurve, AccAttrs, NativeSnapshot.Hit, NativeSnapshot.HitTime, Bones);
		}

		Output.Pose.CopyBonesFrom(AccPose);
		Output.Curve.CopyFrom(AccCurve);
		return true;
	}

	FKodNativePoseSnapshot NativeSnapshot;
};

FAnimInstanceProxy* UKodUnitAnimInstance::CreateAnimInstanceProxy()
{
	return new FKodUnitAnimInstanceProxy(this);
}

void UKodUnitAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy)
{
	delete InProxy;
}

void UKodUnitAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	if (AKodUnit* UnitPawn = Cast<AKodUnit>(TryGetPawnOwner()))
	{
		UnitPawn->InitializeAnimFromDefinition(this);
	}
}

void UKodUnitAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (bHitReactClearNextUpdate)
	{
		bHitReact = false;
		bHitReactClearNextUpdate = false;
	}

	AKodUnit* UnitPawn = Cast<AKodUnit>(TryGetPawnOwner());
	ReadSimPresentation(UnitPawn);
	if (bNativePoseDriver)
	{
		AdvanceNativePose(DeltaSeconds);
	}

	if (bHitReact)
	{
		bHitReactClearNextUpdate = true;
	}
}

void UKodUnitAnimInstance::SetupFromDefinition(const UKodUnitDefinition* Def, bool bNativeDriver)
{
	bNativePoseDriver = bNativeDriver;
	if (!Def)
	{
		return;
	}

	auto LoadSeq = [](const TSoftObjectPtr<UAnimSequence>& Soft) -> UAnimSequence*
	{
		return Soft.IsNull() ? nullptr : Soft.LoadSynchronous();
	};

	NativeIdle = LoadSeq(Def->IdleAnim);
	NativeWalk = LoadSeq(Def->WalkAnim);
	NativeRun = LoadSeq(Def->RunAnim);
	NativeAim = LoadSeq(Def->AimIdleAnim);
	NativeFire = LoadSeq(Def->FireAnim);
	NativeHit = LoadSeq(Def->HitReactAnim);
	NativeDeath = LoadSeq(Def->DeathAnim);
	NativeDeathB = LoadSeq(Def->DeathAnimB);
	DeathAnim = NativeDeath;
	DeathAnimB = NativeDeathB;
	DeathHoldFrame = Def->DeathHoldFrame;
	DeathHoldFrameB = Def->DeathHoldFrameB;
	ShotSequenceTime = FindShotSequenceTime(NativeFire);
}

float UKodUnitAnimInstance::FindShotSequenceTime(const UAnimSequence* FireSeq)
{
	if (!FireSeq)
	{
		return ShotFrameSeconds;
	}
	for (const FAnimNotifyEvent& Evt : FireSeq->Notifies)
	{
		if (Evt.Notify && Evt.Notify->IsA(UKodAnimNotify_Shot::StaticClass()))
		{
			return FMath::Max(0.f, Evt.GetTriggerTime());
		}
	}
	return ShotFrameSeconds;
}

void UKodUnitAnimInstance::AdvanceNativePose(float DeltaSeconds)
{
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	const float BlendStep = (NativeBlendSeconds > KINDA_SMALL_NUMBER) ? (1.f / NativeBlendSeconds) : 0.f;
	const bool bWantAim = bIsAiming && Speed <= AimStationarySpeed && NativeAim != nullptr;
	AimBlend = FMath::FInterpConstantTo(AimBlend, bWantAim ? 1.f : 0.f, Dt, BlendStep);
	DeathBlend = FMath::FInterpConstantTo(DeathBlend, bIsDead ? 1.f : 0.f, Dt, BlendStep);

	auto AdvanceLoop = [Dt](float& Time, const UAnimSequence* Sequence, float Rate)
	{
		if (!Sequence)
		{
			return;
		}
		const float Length = Sequence->GetPlayLength();
		if (Length <= KINDA_SMALL_NUMBER)
		{
			Time = 0.f;
			return;
		}
		Time += Dt * FMath::Max(0.f, Rate);
		Time = FMath::Fmod(Time, Length);
		if (Time < 0.f)
		{
			Time += Length;
		}
	};

	const float WalkRate = Speed / FMath::Max(1.f, AuthoredWalkSpeed);
	const float RunRate = Speed / FMath::Max(1.f, AuthoredRunSpeed);
	AdvanceLoop(IdleTime, NativeIdle, 1.f);
	AdvanceLoop(WalkTime, NativeWalk, WalkRate);
	AdvanceLoop(RunTime, NativeRun, RunRate);
	AdvanceLoop(AimTime, NativeAim, 1.f);

	if (bFireActive && NativeFire)
	{
		const float Length = NativeFire->GetPlayLength();
		FireTime += Dt * FMath::Max(0.f, FirePlayRate);
		if (!bShotFired && FireTime >= ShotSequenceTime)
		{
			bShotFired = true;
			if (AKodUnit* UnitPawn = Cast<AKodUnit>(TryGetPawnOwner()))
			{
				UnitPawn->PlayMuzzleFromShotNotify();
			}
		}
		const float BlendSpan = NativeBlendSeconds * FMath::Max(FirePlayRate, 0.01f);
		float Blend = 1.f;
		if (Length > KINDA_SMALL_NUMBER && BlendSpan > KINDA_SMALL_NUMBER)
		{
			if (FireTime < BlendSpan)
			{
				Blend = FireTime / BlendSpan;
			}
			else if (Length - FireTime < BlendSpan)
			{
				Blend = FMath::Max(0.f, (Length - FireTime) / BlendSpan);
			}
		}
		FireBlend = FMath::Clamp(Blend, 0.f, 1.f);
		if (Length <= KINDA_SMALL_NUMBER || FireTime >= Length)
		{
			bFireActive = false;
			FireBlend = 0.f;
		}
	}
	else
	{
		FireBlend = 0.f;
	}

	if (bHitActive && NativeHit)
	{
		HitTime += Dt;
		if (HitTime >= NativeHit->GetPlayLength())
		{
			bHitActive = false;
		}
	}

	if (bIsDead)
	{
		const UAnimSequence* DeathSeq = (bDeathVariantB && NativeDeathB) ? NativeDeathB.Get() : NativeDeath.Get();
		if (!DeathSeq)
		{
			DeathSeq = NativeDeathB ? NativeDeathB.Get() : NativeDeath.Get();
		}
		if (DeathSeq)
		{
			DeathTime = FMath::Min(DeathTime + Dt, DeathSeq->GetPlayLength());
		}
	}
}

void UKodUnitAnimInstance::ReadSimPresentation(AKodUnit* UnitPawn)
{
	float NewSpeed = 0.f;
	bool bAim = false;
	bool bDead = bIsDead;

	if (UnitPawn)
	{
		bDead = UnitPawn->IsDeathSinking();
		if (UnitPawn->EntityId.IsValid())
		{
			bDeathVariantB = (UnitPawn->EntityId.Value & 1) != 0;
		}
		if (UWorld* World = UnitPawn->GetWorld())
		{
			if (UKodSimSubsystem* Sim = World->GetSubsystem<UKodSimSubsystem>())
			{
				FKodSimEntityState SimState;
				if (UnitPawn->EntityId.IsValid() && Sim->TryGetState(UnitPawn->EntityId, SimState))
				{
					bAim = SimState.Order == EKodSimOrderType::Attack && SimState.AttackTarget.IsValid();
					if (SimState.Order == EKodSimOrderType::Move)
					{
						const float Accept = FMath::Max(1.f, SimState.AcceptanceRadius * 0.5f);
						const float DistSq = FVector::DistSquared2D(SimState.Position, SimState.MoveTarget);
						NewSpeed = (DistSq <= FMath::Square(Accept)) ? 0.f : SimState.MoveSpeed;
					}
					else if (bAim)
					{
						FKodSimEntityState TargetState;
						if (Sim->TryGetState(SimState.AttackTarget, TargetState))
						{
							const float Range = FMath::Max(0.f, SimState.WeaponRange);
							const float DistSq = FVector::DistSquared2D(SimState.Position, TargetState.Position);
							if (DistSq > FMath::Square(Range))
							{
								NewSpeed = SimState.MoveSpeed;
							}
						}
					}
				}
			}
		}
	}

	Speed = FMath::Max(0.f, NewSpeed);
	bIsAiming = bAim;
	bIsDead = bDead;
	const float RunAuthored = FMath::Max(1.f, AuthoredRunSpeed);
	const float WalkAuthored = FMath::Max(1.f, AuthoredWalkSpeed);
	RunPlayRate = Speed / RunAuthored;
	WalkPlayRate = Speed / WalkAuthored;
}

float UKodUnitAnimInstance::GetDeathSinkDelaySeconds() const
{
	const bool bHasDeath = (NativeDeath != nullptr) || (DeathAnim != nullptr);
	const bool bHasDeathB = (NativeDeathB != nullptr) || (DeathAnimB != nullptr);
	if (!bHasDeath && !bHasDeathB)
	{
		return 0.f;
	}

	bool bPlayB = bDeathVariantB;
	if (bPlayB && !bHasDeathB && bHasDeath)
	{
		bPlayB = false;
	}
	else if (!bPlayB && !bHasDeath && bHasDeathB)
	{
		bPlayB = true;
	}

	const int32 HoldFrame = bPlayB ? DeathHoldFrameB : DeathHoldFrame;
	const float FrameRate = FMath::Max(1.f, AuthoredFrameRate);
	const float HoldSeconds = static_cast<float>(FMath::Max(0, HoldFrame)) / FrameRate;
	return HoldSeconds + FMath::Max(0.f, DeathSinkAfterHoldSeconds);
}

void UKodUnitAnimInstance::HandleSimFired()
{
	++FireCounter;
	OnFire.Broadcast();
	if (bNativePoseDriver && NativeFire)
	{
		bFireActive = true;
		bShotFired = false;
		FireTime = 0.f;
		FireBlend = 0.f;
		ShotSequenceTime = FindShotSequenceTime(NativeFire);
	}
}

void UKodUnitAnimInstance::HandleSimHit()
{
	bHitReact = true;
	bHitReactClearNextUpdate = false;
	if (bNativePoseDriver && NativeHit)
	{
		bHitActive = true;
		HitTime = 0.f;
	}
}

void UKodUnitAnimInstance::HandleSimDeath()
{
	bIsDead = true;
	bIsAiming = false;
	bHitReact = false;
	if (const AKodUnit* UnitPawn = Cast<AKodUnit>(TryGetPawnOwner()))
	{
		if (UnitPawn->EntityId.IsValid())
		{
			bDeathVariantB = (UnitPawn->EntityId.Value & 1) != 0;
		}
	}
}
