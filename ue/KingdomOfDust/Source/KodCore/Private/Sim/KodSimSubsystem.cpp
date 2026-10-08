#include "Sim/KodSimSubsystem.h"
#include "GameFramework/Actor.h"
#include "Hash/CityHash.h"

void UKodSimSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	NextId = 1;
	Entities.Reset();
	States.Reset();
	PrevPositions.Reset();
	PrevYaws.Reset();
	TimeAccumulator = 0.f;
	SimTickIndex = 0;
	SimHz = KodBuildTicks::SimHz;
	MaxCatchUpSteps = KodBuildTicks::MaxCatchUpSteps;
}

void UKodSimSubsystem::Deinitialize()
{
	Entities.Reset();
	States.Reset();
	PrevPositions.Reset();
	PrevYaws.Reset();
	Super::Deinitialize();
}

TStatId UKodSimSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKodSimSubsystem, STATGROUP_Tickables);
}

void UKodSimSubsystem::Tick(float DeltaTime)
{
	const float FixedDt = (SimHz > 0) ? (1.f / static_cast<float>(SimHz)) : KodBuildTicks::SimDt;
	TimeAccumulator += DeltaTime;

	int32 Steps = 0;
	while (TimeAccumulator >= FixedDt && Steps < MaxCatchUpSteps)
	{
		// Snapshot previous poses for presentation lerp
		PrevPositions.Reset();
		PrevYaws.Reset();
		for (const TPair<int32, FKodSimEntityState>& Pair : States)
		{
			PrevPositions.Add(Pair.Key, Pair.Value.Position);
			PrevYaws.Add(Pair.Key, Pair.Value.YawDegrees);
		}

		StepSim(FixedDt);
		TimeAccumulator -= FixedDt;
		++Steps;
		++SimTickIndex;
	}

	// Avoid spiral: drop excess accumulated time beyond one step
	if (TimeAccumulator > FixedDt)
	{
		TimeAccumulator = FixedDt;
	}

	const float Alpha = FixedDt > 0.f ? FMath::Clamp(TimeAccumulator / FixedDt, 0.f, 1.f) : 1.f;
	SyncActorPresentation(Alpha);
}

void UKodSimSubsystem::StepSim(float FixedDt)
{
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();

	for (int32 Key : Keys)
	{
		FKodSimEntityState* State = States.Find(Key);
		if (!State)
		{
			continue;
		}
		if (State->CooldownRemaining > 0.f)
		{
			State->CooldownRemaining = FMath::Max(0.f, State->CooldownRemaining - FixedDt);
		}

		switch (State->Order)
		{
		case EKodSimOrderType::Move:
			StepEntityMove(*State, FixedDt);
			break;
		case EKodSimOrderType::Attack:
			StepEntityAttack(*State, FixedDt);
			break;
		default:
			// Idle: do not micro-integrate — keep quantized pose stable for hash
			QuantizePose(*State);
			break;
		}
	}
}

void UKodSimSubsystem::StepEntityMove(FKodSimEntityState& State, float FixedDt)
{
	if (!State.bMobile || State.MoveSpeed <= 0.f)
	{
		State.Order = EKodSimOrderType::None;
		QuantizePose(State);
		return;
	}

	const FVector ToTarget = State.MoveTarget - State.Position;
	const float DistSq = ToTarget.SizeSquared2D();
	const float Accept = FMath::Max(1.f, State.AcceptanceRadius * 0.5f);
	if (DistSq <= FMath::Square(Accept))
	{
		State.Position.X = State.MoveTarget.X;
		State.Position.Y = State.MoveTarget.Y;
		State.Order = EKodSimOrderType::None;
		QuantizePose(State);
		return;
	}

	FVector Dir = ToTarget;
	Dir.Z = 0.f;
	Dir.Normalize();
	State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	State.Position += Dir * (State.MoveSpeed * FixedDt);
	// Keep Z from existing (ground projection is presentation/assist later — no NavMesh)
}

void UKodSimSubsystem::StepEntityAttack(FKodSimEntityState& State, float FixedDt)
{
	FKodSimEntityState* Target = States.Find(State.AttackTarget.Value);
	if (!Target || Target->Health <= 0.f)
	{
		State.Order = EKodSimOrderType::None;
		State.AttackTarget = FKodEntityId();
		QuantizePose(State);
		return;
	}

	const float DistSq = FVector::DistSquared2D(State.Position, Target->Position);
	const float Range = FMath::Max(0.f, State.WeaponRange);
	if (DistSq > FMath::Square(Range))
	{
		// Seek into range (sim seek — not AI MoveTo / PathFollowing)
		State.MoveTarget = Target->Position;
		StepEntityMove(State, FixedDt);
		State.Order = EKodSimOrderType::Attack; // restore after seek helper may clear
		State.AttackTarget = Target->Id;
		return;
	}

	// Face target
	const FVector ToTarget = Target->Position - State.Position;
	State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));

	if (State.CooldownRemaining <= 0.f && State.WeaponDamage > 0.f)
	{
		// Hitscan — no actor bullets. Slice 0: raw Damage only; Target Armor not applied yet (combat pass).
		// HP 0 stays registered. There is no despawn pass; the next step clears this order.
		const float Mitigated = FMath::Max(0.f, State.WeaponDamage);
		Target->Health = FMath::Max(0.f, Target->Health - Mitigated);
		State.CooldownRemaining = FMath::Max(0.01f, State.WeaponCooldownSeconds);

		const TCHAR* TargetName = TEXT("None");
		FString NameStorage;
		if (const TWeakObjectPtr<AActor>* Found = Entities.Find(Target->Id.Value))
		{
			if (const AActor* TargetActor = Found->Get())
			{
				NameStorage = TargetActor->GetName();
				TargetName = *NameStorage;
			}
		}
		UE_LOG(LogTemp, Log, TEXT("KodSim Hit Target=%s Id=%d HP=%.0f"),
			TargetName,
			Target->Id.Value,
			Target->Health);
	}

	QuantizePose(State);
}

void UKodSimSubsystem::QuantizePose(FKodSimEntityState& State) const
{
	const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	State.Position.X = FMath::RoundToFloat(State.Position.X / Q) * Q;
	State.Position.Y = FMath::RoundToFloat(State.Position.Y / Q) * Q;
	State.Position.Z = FMath::RoundToFloat(State.Position.Z / Q) * Q;
	State.YawDegrees = FMath::RoundToFloat(State.YawDegrees * 100.f) / 100.f;
}

void UKodSimSubsystem::SyncActorPresentation(float Alpha) const
{
	for (const TPair<int32, FKodSimEntityState>& Pair : States)
	{
		const TWeakObjectPtr<AActor>* ActorPtr = Entities.Find(Pair.Key);
		AActor* Actor = ActorPtr ? ActorPtr->Get() : nullptr;
		if (!Actor)
		{
			continue;
		}
		const FVector* Prev = PrevPositions.Find(Pair.Key);
		const FVector From = Prev ? *Prev : Pair.Value.Position;
		const FVector To = Pair.Value.Position;
		const FVector Lerped = FMath::Lerp(From, To, Alpha);
		Actor->SetActorLocation(Lerped, false, nullptr, ETeleportType::None);

		const float* PrevYaw = PrevYaws.Find(Pair.Key);
		const float YawFrom = PrevYaw ? *PrevYaw : Pair.Value.YawDegrees;
		const float YawTo = Pair.Value.YawDegrees;
		const float Yaw = FMath::Lerp(YawFrom, YawTo, Alpha);
		FRotator Rot = Actor->GetActorRotation();
		Rot.Yaw = Yaw;
		Actor->SetActorRotation(Rot);
	}
}

FKodEntityId UKodSimSubsystem::RegisterEntity(AActor* Actor)
{
	FKodEntityId Id;
	if (!Actor)
	{
		return Id;
	}
	Id.Value = NextId++;
	Entities.Add(Id.Value, Actor);

	FKodSimEntityState State;
	State.Id = Id;
	State.Position = Actor->GetActorLocation();
	State.YawDegrees = Actor->GetActorRotation().Yaw;
	QuantizePose(State);
	States.Add(Id.Value, State);
	PrevPositions.Add(Id.Value, State.Position);
	PrevYaws.Add(Id.Value, State.YawDegrees);
	return Id;
}

void UKodSimSubsystem::UnregisterEntity(FKodEntityId Id)
{
	Entities.Remove(Id.Value);
	States.Remove(Id.Value);
	PrevPositions.Remove(Id.Value);
	PrevYaws.Remove(Id.Value);
}

AActor* UKodSimSubsystem::ResolveEntity(FKodEntityId Id) const
{
	if (const TWeakObjectPtr<AActor>* Found = Entities.Find(Id.Value))
	{
		return Found->Get();
	}
	return nullptr;
}

FKodEntityId UKodSimSubsystem::FindIdForActor(AActor* Actor) const
{
	FKodEntityId Result;
	if (!Actor)
	{
		return Result;
	}
	for (const TPair<int32, TWeakObjectPtr<AActor>>& Pair : Entities)
	{
		if (Pair.Value.Get() == Actor)
		{
			Result.Value = Pair.Key;
			return Result;
		}
	}
	return Result;
}

void UKodSimSubsystem::ConfigureEntity(
	FKodEntityId Id,
	FName DefinitionId,
	float MaxHealth,
	float MoveSpeed,
	float AcceptanceRadius,
	bool bMobile,
	float WeaponDamage,
	float WeaponRange,
	float WeaponCooldownSeconds,
	int32 TeamId)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return;
	}
	State->DefinitionId = DefinitionId;
	State->MaxHealth = MaxHealth;
	State->Health = MaxHealth; // HP from DA only at configure time
	State->MoveSpeed = MoveSpeed;
	State->AcceptanceRadius = AcceptanceRadius;
	State->bMobile = bMobile;
	State->WeaponDamage = WeaponDamage;
	State->WeaponRange = WeaponRange;
	State->WeaponCooldownSeconds = WeaponCooldownSeconds;
	State->TeamId = TeamId;
}

void UKodSimSubsystem::IssueMove(FKodEntityId Id, FVector WorldLocation)
{
	if (FKodSimEntityState* State = States.Find(Id.Value))
	{
		State->Order = EKodSimOrderType::Move;
		State->MoveTarget = WorldLocation;
		State->AttackTarget = FKodEntityId();
	}
}

void UKodSimSubsystem::IssueAttack(FKodEntityId Id, FKodEntityId TargetId)
{
	if (FKodSimEntityState* State = States.Find(Id.Value))
	{
		State->Order = EKodSimOrderType::Attack;
		State->AttackTarget = TargetId;
	}
}

void UKodSimSubsystem::IssueStop(FKodEntityId Id)
{
	if (FKodSimEntityState* State = States.Find(Id.Value))
	{
		State->Order = EKodSimOrderType::None;
		State->AttackTarget = FKodEntityId();
		QuantizePose(*State);
	}
}

bool UKodSimSubsystem::TryGetState(FKodEntityId Id, FKodSimEntityState& OutState) const
{
	if (const FKodSimEntityState* State = States.Find(Id.Value))
	{
		OutState = *State;
		return true;
	}
	return false;
}

FVector UKodSimSubsystem::GetPresentationPosition(FKodEntityId Id, float Alpha) const
{
	const FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return FVector::ZeroVector;
	}
	const FVector* Prev = PrevPositions.Find(Id.Value);
	const FVector From = Prev ? *Prev : State->Position;
	return FMath::Lerp(From, State->Position, Alpha);
}

int32 UKodSimSubsystem::GetPendingOrderCount() const
{
	int32 Count = 0;
	for (const TPair<int32, FKodSimEntityState>& Pair : States)
	{
		if (Pair.Value.HasPendingOrder())
		{
			++Count;
		}
	}
	return Count;
}

bool UKodSimSubsystem::IsWorldIdle() const
{
	return GetPendingOrderCount() == 0;
}

FString UKodSimSubsystem::ComputeIdleHash() const
{
	// Stable when idle (no orders / no micro-integrate). Quantized pose + DA-driven HP.
	// TeamId is sim state but omitted so this byte layout stays ids / pose / HP / order.
	// Visual bob and actor yaw are presentation and must not be appended here.
	TArray<uint8> Bytes;
	auto Append = [&Bytes](const void* Data, int32 Size)
	{
		const int32 Offset = Bytes.Num();
		Bytes.AddUninitialized(Size);
		FMemory::Memcpy(Bytes.GetData() + Offset, Data, Size);
	};

	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();

	for (int32 Key : Keys)
	{
		const FKodSimEntityState& S = States.FindChecked(Key);
		Append(&Key, sizeof(Key));

		const int32 DefHash = GetTypeHash(S.DefinitionId);
		Append(&DefHash, sizeof(DefHash));

		const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
		const int32 QX = FMath::RoundToInt(S.Position.X / Q);
		const int32 QY = FMath::RoundToInt(S.Position.Y / Q);
		const int32 QZ = FMath::RoundToInt(S.Position.Z / Q);
		Append(&QX, sizeof(QX));
		Append(&QY, sizeof(QY));
		Append(&QZ, sizeof(QZ));

		const int32 HP = FMath::RoundToInt(S.Health);
		const int32 MaxHP = FMath::RoundToInt(S.MaxHealth);
		Append(&HP, sizeof(HP));
		Append(&MaxHP, sizeof(MaxHP));

		const uint8 OrderByte = static_cast<uint8>(S.Order);
		Append(&OrderByte, sizeof(OrderByte));
	}

	const uint64 Hash = Bytes.Num() > 0
		? CityHash64(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num())
		: 0ull;
	return FString::Printf(TEXT("%016llX"), Hash);
}
