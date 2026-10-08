#include "Animation/KodUnitAnimInstance.h"
#include "Actors/KodUnit.h"
#include "Sim/KodSimSubsystem.h"

void UKodUnitAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	(void)DeltaSeconds;

	if (bHitReactClearNextUpdate)
	{
		bHitReact = false;
		bHitReactClearNextUpdate = false;
	}

	AKodUnit* UnitPawn = Cast<AKodUnit>(TryGetPawnOwner());
	ReadSimPresentation(UnitPawn);

	if (bHitReact)
	{
		// The graph runs after this function and still sees the pulse.
		bHitReactClearNextUpdate = true;
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
	const bool bHasDeath = DeathAnim != nullptr;
	const bool bHasDeathB = DeathAnimB != nullptr;
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
}

void UKodUnitAnimInstance::HandleSimHit()
{
	bHitReact = true;
	bHitReactClearNextUpdate = false;
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
