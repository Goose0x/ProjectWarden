#include "Sim/KodSimSubsystem.h"
#include "Game/KodPlayerController.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Hash/CityHash.h"

void UKodSimSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	NextId = 1;
	Entities.Reset();
	States.Reset();
	PrevPositions.Reset();
	PrevYaws.Reset();
	Banks.Reset();
	ResourceNodes.Reset();
	DropOffs.Reset();
	TimeAccumulator = 0.f;
	SimTickIndex = 0;
	SimHz = KodBuildTicks::SimHz;
	MaxCatchUpSteps = KodBuildTicks::MaxCatchUpSteps;
	SetTeamBank(0, StartingJadeite, StartingLuminene);
}

void UKodSimSubsystem::Deinitialize()
{
	OnUnitFired.Clear();
	OnUnitHit.Clear();
	OnUnitKilled.Clear();
	Entities.Reset();
	States.Reset();
	Banks.Reset();
	ResourceNodes.Reset();
	DropOffs.Reset();
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

		if (State->GatherPhase != EKodGatherPhase::None || State->Order == EKodSimOrderType::Gather)
		{
			// Do not use State after this if a later unregister rehashes. Gather does not unregister workers.
			StepEntityGather(*State, FixedDt);
			continue;
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
			// Idle: auto-acquire or hold a quantized pose. An explicit Move is not this branch.
			if (!TryAutoAcquire(*State, FixedDt))
			{
				QuantizePose(*State);
			}
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

	// Seek until the shot can land, then hold. StepEntityMove may clear the order
	// when it arrives inside acceptance; put the attack back before the range recheck.
	const float Range = FMath::Max(0.f, State.WeaponRange);
	float DistSq = FVector::DistSquared2D(State.Position, Target->Position);
	if (DistSq > FMath::Square(Range))
	{
		const FKodEntityId KeptTarget = Target->Id;
		State.MoveTarget = Target->Position;
		StepEntityMove(State, FixedDt);
		State.Order = EKodSimOrderType::Attack;
		State.AttackTarget = KeptTarget;
		DistSq = FVector::DistSquared2D(State.Position, Target->Position);
		if (DistSq > FMath::Square(Range))
		{
			return;
		}
	}

	// In range: stop integrating and face the target. Presentation chases this yaw.
	const FVector ToTarget = Target->Position - State.Position;
	if (ToTarget.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
	}

	if (State.CooldownRemaining <= 0.f && State.WeaponDamage > 0.f)
	{
		// Hitscan — no actor bullets. Slice 0: raw Damage only; target Armor is not applied yet.
		auto ActorOf = [this](int32 IdValue) -> AActor*
		{
			if (const TWeakObjectPtr<AActor>* Found = Entities.Find(IdValue))
			{
				return Found->Get();
			}
			return nullptr;
		};
		auto NameOf = [](const AActor* NamedActor) -> FString
		{
			return NamedActor ? NamedActor->GetName() : FString(TEXT("None"));
		};

		AActor* AttackerActor = ActorOf(State.Id.Value);
		AActor* TargetActor = ActorOf(Target->Id.Value);
		const FString AttackerName = NameOf(AttackerActor);
		const FString TargetName = NameOf(TargetActor);
		const int32 TargetIdValue = Target->Id.Value;
		const FKodEntityId DeadId = Target->Id;

		const float Mitigated = FMath::Max(0.f, State.WeaponDamage);
		Target->Health = FMath::Max(0.f, Target->Health - Mitigated);
		State.CooldownRemaining = FMath::Max(0.01f, State.WeaponCooldownSeconds);
		const float HealthLeft = Target->Health;

		// Hit while idle: retaliate next acquire, even if the attacker is outside range.
		// A unit that already has Move or Attack keeps that order.
		if (HealthLeft > 0.f
			&& Target->Order == EKodSimOrderType::None
			&& Target->WeaponDamage > 0.f
			&& Target->TeamId != State.TeamId)
		{
			Target->RetaliateTarget = State.Id;
		}

		UE_LOG(LogTemp, Log, TEXT("KodSim Fire Attacker=%s Target=%s Tick=%lld"),
			*AttackerName,
			*TargetName,
			static_cast<long long>(SimTickIndex));
		OnUnitFired.Broadcast(AttackerActor, TargetActor, static_cast<int32>(SimTickIndex));

		UE_LOG(LogTemp, Log, TEXT("KodSim Hit Target=%s Id=%d HP=%.0f"),
			*TargetName,
			TargetIdValue,
			HealthLeft);
		OnUnitHit.Broadcast(AttackerActor, TargetActor, HealthLeft, TargetIdValue);

		if (HealthLeft <= 0.f)
		{
			UE_LOG(LogTemp, Log, TEXT("KodSim Kill Target=%s Id=%d"),
				*TargetName,
				TargetIdValue);
			State.Order = EKodSimOrderType::None;
			State.AttackTarget = FKodEntityId();
			QuantizePose(State);
			// Remove before the delegate so listeners cannot select or attack the corpse.
			// Do not touch Target or State after this; TMap::Remove can rehash.
			UnregisterEntity(DeadId);
			OnUnitKilled.Broadcast(TargetActor, TargetIdValue);
			return;
		}
	}

	QuantizePose(State);
}

FString UKodSimSubsystem::GetEntityActorName(int32 IdValue) const
{
	if (const TWeakObjectPtr<AActor>* Found = Entities.Find(IdValue))
	{
		if (const AActor* NamedActor = Found->Get())
		{
			return NamedActor->GetName();
		}
	}
	return FString(TEXT("None"));
}

FKodEntityId UKodSimSubsystem::FindNearestEnemyInRange(const FKodSimEntityState& State) const
{
	// Resource nodes are not in States, so they are not enemies and do not auto-acquire.
	FKodEntityId BestId;
	float BestDistSq = 0.f;
	bool bFound = false;
	const float RangeSq = FMath::Square(FMath::Max(0.f, State.WeaponRange));

	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		if (Key == State.Id.Value)
		{
			continue;
		}
		const FKodSimEntityState* Other = States.Find(Key);
		if (!Other || Other->Health <= 0.f || Other->TeamId == State.TeamId)
		{
			continue;
		}
		// Unarmed workers stay out of the marine duel. RMB Attack can still target them.
		if (Other->bCanGather && Other->WeaponDamage <= 0.f)
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(State.Position, Other->Position);
		if (DistSq > RangeSq)
		{
			continue;
		}
		// Keys are sorted, so the first unit at a distance is the lowest id.
		// A later id replaces it only when strictly closer.
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			BestId.Value = Key;
		}
	}
	return BestId;
}

bool UKodSimSubsystem::TryAutoAcquire(FKodSimEntityState& State, float FixedDt)
{
	if (State.Order != EKodSimOrderType::None || State.WeaponDamage <= 0.f || State.Health <= 0.f)
	{
		return false;
	}

	FKodEntityId Chosen;
	const TCHAR* Reason = nullptr;

	if (State.RetaliateTarget.IsValid())
	{
		const FKodEntityId Pending = State.RetaliateTarget;
		State.RetaliateTarget = FKodEntityId();
		if (const FKodSimEntityState* Aggro = States.Find(Pending.Value))
		{
			if (Aggro->Health > 0.f && Aggro->TeamId != State.TeamId && Aggro->Id.Value != State.Id.Value)
			{
				Chosen = Aggro->Id;
				Reason = TEXT("Retaliate");
			}
		}
	}

	if (!Chosen.IsValid())
	{
		Chosen = FindNearestEnemyInRange(State);
		if (Chosen.IsValid())
		{
			Reason = TEXT("InRange");
		}
	}

	if (!Chosen.IsValid() || !Reason)
	{
		return false;
	}

	State.Order = EKodSimOrderType::Attack;
	State.AttackTarget = Chosen;
	UE_LOG(LogTemp, Log, TEXT("KodSim AutoAcquire Unit=%s Target=%s Reason=%s"),
		*GetEntityActorName(State.Id.Value),
		*GetEntityActorName(Chosen.Value),
		Reason);
	// Same hitscan path as an explicit attack. May unregister the target.
	// Do not use State after this call; TMap::Remove can rehash.
	StepEntityAttack(State, FixedDt);
	return true;
}

void UKodSimSubsystem::QuantizePose(FKodSimEntityState& State) const
{
	QuantizeResourcePosition(State.Position);
	State.YawDegrees = FMath::RoundToFloat(State.YawDegrees * 100.f) / 100.f;
}

void UKodSimSubsystem::QuantizeResourcePosition(FVector& Position) const
{
	const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	Position.X = FMath::RoundToFloat(Position.X / Q) * Q;
	Position.Y = FMath::RoundToFloat(Position.Y / Q) * Q;
	Position.Z = FMath::RoundToFloat(Position.Z / Q) * Q;
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
		const float Yaw = YawFrom + FMath::FindDeltaAngleDegrees(YawFrom, YawTo) * Alpha;
		FRotator Rot = Actor->GetActorRotation();
		Rot.Yaw = Yaw;
		// None keeps the previous transform so motion vectors stay continuous.
		// TeleportPhysics would zero them and smear the skinned mesh under TAA.
		Actor->SetActorRotation(Rot, ETeleportType::None);
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
	int32 TeamId,
	bool bCanGather)
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
	State->bCanGather = bCanGather;
}

namespace
{
	void FaceStateToward(FKodSimEntityState& State, const FVector& WorldPoint)
	{
		FVector Dir = WorldPoint - State.Position;
		Dir.Z = 0.f;
		if (Dir.SizeSquared() <= KINDA_SMALL_NUMBER)
		{
			return;
		}
		State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	}
}

void UKodSimSubsystem::IssueMove(FKodEntityId Id, FVector WorldLocation)
{
	if (FKodSimEntityState* State = States.Find(Id.Value))
	{
		if (State->GatherPhase != EKodGatherPhase::None || State->IsCarryingCargo())
		{
			RefundGatherCargo(*State);
			ClearGatherFields(*State);
		}
		State->Order = EKodSimOrderType::Move;
		State->MoveTarget = WorldLocation;
		State->AttackTarget = FKodEntityId();
		State->RetaliateTarget = FKodEntityId();
		FaceStateToward(*State, WorldLocation);
	}
}

void UKodSimSubsystem::IssueAttack(FKodEntityId Id, FKodEntityId TargetId)
{
	// A node is a gather/move destination. A drop-off is not a target. Do not open an attack onto either.
	if (IsResourceNode(TargetId) || IsDropOff(TargetId))
	{
		return;
	}
	if (FKodSimEntityState* State = States.Find(Id.Value))
	{
		if (State->GatherPhase != EKodGatherPhase::None || State->IsCarryingCargo())
		{
			RefundGatherCargo(*State);
			ClearGatherFields(*State);
		}
		State->Order = EKodSimOrderType::Attack;
		State->AttackTarget = TargetId;
		State->RetaliateTarget = FKodEntityId();
		if (const FKodSimEntityState* TargetState = States.Find(TargetId.Value))
		{
			FaceStateToward(*State, TargetState->Position);
		}
	}
}

void UKodSimSubsystem::IssueStop(FKodEntityId Id)
{
	if (FKodSimEntityState* State = States.Find(Id.Value))
	{
		if (State->GatherPhase != EKodGatherPhase::None || State->IsCarryingCargo())
		{
			RefundGatherCargo(*State);
			ClearGatherFields(*State);
		}
		State->Order = EKodSimOrderType::None;
		State->AttackTarget = FKodEntityId();
		State->RetaliateTarget = FKodEntityId();
		QuantizePose(*State);
	}
}

namespace
{
	int32 SaturatingAddNonNegative(int32 Have, int32 Add)
	{
		if (Add <= 0)
		{
			return Have;
		}
		if (Have > MAX_int32 - Add)
		{
			return MAX_int32;
		}
		return Have + Add;
	}
}

void UKodSimSubsystem::SetTeamBank(int32 TeamId, int32 Jadeite, int32 Luminene)
{
	FKodResourceCost& Bank = Banks.FindOrAdd(TeamId);
	Bank.Jadeite = FMath::Max(0, Jadeite);
	Bank.Luminene = FMath::Max(0, Luminene);
}

bool UKodSimSubsystem::TryGetBank(int32 TeamId, FKodResourceCost& OutBank) const
{
	if (const FKodResourceCost* Bank = Banks.Find(TeamId))
	{
		OutBank = *Bank;
		return true;
	}
	OutBank = FKodResourceCost();
	return false;
}

bool UKodSimSubsystem::CanAfford(int32 TeamId, FKodResourceCost Cost) const
{
	const int32 NeedJadeite = FMath::Max(0, Cost.Jadeite);
	const int32 NeedLuminene = FMath::Max(0, Cost.Luminene);
	FKodResourceCost Bank;
	TryGetBank(TeamId, Bank);
	return Bank.Jadeite >= NeedJadeite && Bank.Luminene >= NeedLuminene;
}

bool UKodSimSubsystem::Spend(int32 TeamId, FKodResourceCost Cost)
{
	const int32 NeedJadeite = FMath::Max(0, Cost.Jadeite);
	const int32 NeedLuminene = FMath::Max(0, Cost.Luminene);
	FKodResourceCost* Bank = Banks.Find(TeamId);
	const int32 HaveJadeite = Bank ? Bank->Jadeite : 0;
	const int32 HaveLuminene = Bank ? Bank->Luminene : 0;
	if (HaveJadeite < NeedJadeite || HaveLuminene < NeedLuminene)
	{
		UE_LOG(LogTemp, Log, TEXT("KodEcon Spend Reject Team=%d J=%d L=%d Bank=J%d,L%d"),
			TeamId,
			NeedJadeite,
			NeedLuminene,
			HaveJadeite,
			HaveLuminene);
		return false;
	}
	if (NeedJadeite == 0 && NeedLuminene == 0)
	{
		return true;
	}
	if (!Bank)
	{
		return false;
	}
	Bank->Jadeite -= NeedJadeite;
	Bank->Luminene -= NeedLuminene;
	UE_LOG(LogTemp, Log, TEXT("KodEcon Spend Team=%d J=%d L=%d Bank=J%d,L%d"),
		TeamId,
		NeedJadeite,
		NeedLuminene,
		Bank->Jadeite,
		Bank->Luminene);
	return true;
}

void UKodSimSubsystem::Deposit(int32 TeamId, FKodResourceCost Amount)
{
	const int32 AddJadeite = FMath::Max(0, Amount.Jadeite);
	const int32 AddLuminene = FMath::Max(0, Amount.Luminene);
	FKodResourceCost& Bank = Banks.FindOrAdd(TeamId);
	Bank.Jadeite = SaturatingAddNonNegative(Bank.Jadeite, AddJadeite);
	Bank.Luminene = SaturatingAddNonNegative(Bank.Luminene, AddLuminene);
	UE_LOG(LogTemp, Log, TEXT("KodEcon Deposit Team=%d J=%d L=%d"),
		TeamId,
		AddJadeite,
		AddLuminene);
}

FKodEntityId UKodSimSubsystem::RegisterResourceNode(AActor* Actor, const FKodResourceNodeSpec& Spec)
{
	FKodEntityId Id;
	if (!Actor || Spec.Amount <= 0)
	{
		return Id;
	}

	const FKodEntityId Existing = FindIdForActor(Actor);
	if (Existing.IsValid())
	{
		return IsResourceNode(Existing) ? Existing : Id;
	}

	Id.Value = NextId++;
	Entities.Add(Id.Value, Actor);

	FKodResourceNodeState Node;
	Node.Id = Id;
	Node.Type = Spec.Type;
	Node.DefinitionId = Spec.DefinitionId;
	// Actor origin (the ground point). A mesh-center offset must not live on the root,
	// or this copies (0, 0, half-height) instead of the arc.
	Node.Position = Actor->GetActorLocation();
	QuantizeResourcePosition(Node.Position);
	Node.Remaining = Spec.Amount;
	Node.HarvestPerTrip = FMath::Max(0, Spec.HarvestPerTrip);
	Node.HarvestTicks = FMath::Max(1, Spec.HarvestTicks);
	Node.TricklePerTrip = FMath::Max(0, Spec.TricklePerTrip);
	Node.GatherStandUU = FMath::Max(1, Spec.GatherStandUU);
	Node.bDepletionLogged = false;
	ResourceNodes.Add(Id.Value, Node);
	return Id;
}

FKodEntityId UKodSimSubsystem::RegisterDropOff(AActor* Actor, int32 TeamId, int32 StandUU)
{
	FKodEntityId Id;
	if (!Actor)
	{
		return Id;
	}

	const FKodEntityId Existing = FindIdForActor(Actor);
	if (Existing.IsValid())
	{
		return IsDropOff(Existing) ? Existing : Id;
	}

	Id.Value = NextId++;
	Entities.Add(Id.Value, Actor);

	FKodDropOffState Drop;
	Drop.Id = Id;
	Drop.TeamId = TeamId;
	Drop.Position = Actor->GetActorLocation();
	QuantizeResourcePosition(Drop.Position);
	Drop.StandUU = FMath::Max(1, StandUU);
	DropOffs.Add(Id.Value, Drop);
	UE_LOG(LogTemp, Log, TEXT("KodEcon DropOff Id=%d Team=%d Loc=%.0f,%.0f,%.0f"),
		Id.Value,
		TeamId,
		Drop.Position.X,
		Drop.Position.Y,
		Drop.Position.Z);
	return Id;
}

void UKodSimSubsystem::UnregisterDropOff(FKodEntityId Id)
{
	DropOffs.Remove(Id.Value);
	Entities.Remove(Id.Value);
}

bool UKodSimSubsystem::IsDropOff(FKodEntityId Id) const
{
	return Id.IsValid() && DropOffs.Contains(Id.Value);
}

FKodEntityId UKodSimSubsystem::FindNearestDropOff(int32 TeamId, FVector Origin) const
{
	FKodEntityId Best;
	bool bFound = false;
	int64 BestDistSq = 0;
	const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	const int32 OriginX = FMath::RoundToInt(Origin.X / Q);
	const int32 OriginY = FMath::RoundToInt(Origin.Y / Q);

	TArray<int32> Keys;
	DropOffs.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodDropOffState* Drop = DropOffs.Find(Key);
		if (!Drop || Drop->TeamId != TeamId)
		{
			continue;
		}
		const int32 DropX = FMath::RoundToInt(Drop->Position.X / Q);
		const int32 DropY = FMath::RoundToInt(Drop->Position.Y / Q);
		const int64 DX = static_cast<int64>(DropX) - static_cast<int64>(OriginX);
		const int64 DY = static_cast<int64>(DropY) - static_cast<int64>(OriginY);
		const int64 DistSq = DX * DX + DY * DY;
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			Best.Value = Key;
		}
	}
	return Best;
}

void UKodSimSubsystem::UnregisterResourceNode(FKodEntityId Id)
{
	ResourceNodes.Remove(Id.Value);
	Entities.Remove(Id.Value);
}

bool UKodSimSubsystem::IsResourceNode(FKodEntityId Id) const
{
	return Id.IsValid() && ResourceNodes.Contains(Id.Value);
}

bool UKodSimSubsystem::TryGetResourceNode(FKodEntityId Id, FKodResourceNodeState& OutNode) const
{
	if (const FKodResourceNodeState* Node = ResourceNodes.Find(Id.Value))
	{
		OutNode = *Node;
		return true;
	}
	return false;
}

FKodEntityId UKodSimSubsystem::FindNearestResourceNode(FVector Origin) const
{
	FKodEntityId Best;
	bool bFound = false;
	int64 BestDistSq = 0;
	const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	const int32 OriginX = FMath::RoundToInt(Origin.X / Q);
	const int32 OriginY = FMath::RoundToInt(Origin.Y / Q);

	TArray<int32> Keys;
	ResourceNodes.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodResourceNodeState* Node = ResourceNodes.Find(Key);
		if (!Node || (Node->Remaining <= 0 && Node->TricklePerTrip <= 0))
		{
			continue;
		}
		const int32 NodeX = FMath::RoundToInt(Node->Position.X / Q);
		const int32 NodeY = FMath::RoundToInt(Node->Position.Y / Q);
		const int64 DX = static_cast<int64>(NodeX) - static_cast<int64>(OriginX);
		const int64 DY = static_cast<int64>(NodeY) - static_cast<int64>(OriginY);
		const int64 DistSq = DX * DX + DY * DY;
		// Keys are sorted, so an equal distance keeps the lower id.
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			Best.Value = Key;
		}
	}
	return Best;
}

FKodEntityId UKodSimSubsystem::FindNearestResourceNodeOfType(FVector Origin, EKodResourceType Type, bool bRequireRemaining) const
{
	FKodEntityId Best;
	bool bFound = false;
	int64 BestDistSq = 0;
	const float Q = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	const int32 OriginX = FMath::RoundToInt(Origin.X / Q);
	const int32 OriginY = FMath::RoundToInt(Origin.Y / Q);

	TArray<int32> Keys;
	ResourceNodes.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodResourceNodeState* Node = ResourceNodes.Find(Key);
		if (!Node || Node->Type != Type)
		{
			continue;
		}
		if (bRequireRemaining)
		{
			if (Node->Remaining <= 0)
			{
				continue;
			}
		}
		else if (Node->Remaining <= 0 && Node->TricklePerTrip <= 0)
		{
			continue;
		}
		const int32 NodeX = FMath::RoundToInt(Node->Position.X / Q);
		const int32 NodeY = FMath::RoundToInt(Node->Position.Y / Q);
		const int64 DX = static_cast<int64>(NodeX) - static_cast<int64>(OriginX);
		const int64 DY = static_cast<int64>(NodeY) - static_cast<int64>(OriginY);
		const int64 DistSq = DX * DX + DY * DY;
		if (!bFound || DistSq < BestDistSq)
		{
			bFound = true;
			BestDistSq = DistSq;
			Best.Value = Key;
		}
	}
	return Best;
}

void UKodSimSubsystem::DestroyResourceNodeActor(FKodEntityId Id)
{
	AActor* Actor = ResolveEntity(Id);
	UnregisterResourceNode(Id);
	if (!Actor)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			if (AKodPlayerController* KodPC = Cast<AKodPlayerController>(It->Get()))
			{
				KodPC->RemoveFromLocalSelection(Actor);
			}
		}
	}
	if (!Actor->IsActorBeingDestroyed())
	{
		Actor->Destroy();
	}
}

void UKodSimSubsystem::ClearGatherFields(FKodSimEntityState& State)
{
	State.GatherPhase = EKodGatherPhase::None;
	State.GatherNode = FKodEntityId();
	State.GatherDropOff = FKodEntityId();
	State.GatherType = EKodResourceType::Jadeite;
	State.CargoJadeite = 0;
	State.CargoLuminene = 0;
	State.bCargoFromTrickle = false;
	State.HarvestTicksRemaining = 0;
	State.GatherStallTicks = 0;
	State.bGatherStallRetargeted = false;
	if (State.Order == EKodSimOrderType::Gather)
	{
		State.Order = EKodSimOrderType::None;
	}
}

void UKodSimSubsystem::RefundGatherCargo(FKodSimEntityState& State)
{
	const int32 Jadeite = State.CargoJadeite;
	const int32 Luminene = State.CargoLuminene;
	if (Jadeite <= 0 && Luminene <= 0)
	{
		return;
	}

	const bool bTrickle = State.bCargoFromTrickle;
	const FKodEntityId NodeId = State.GatherNode;
	const int32 TeamId = State.TeamId;
	State.CargoJadeite = 0;
	State.CargoLuminene = 0;
	State.bCargoFromTrickle = false;
	if (bTrickle)
	{
		return;
	}

	if (FKodResourceNodeState* Node = ResourceNodes.Find(NodeId.Value))
	{
		if (Node->Type == EKodResourceType::Luminene)
		{
			Node->Remaining = SaturatingAddNonNegative(Node->Remaining, Luminene);
		}
		else
		{
			Node->Remaining = SaturatingAddNonNegative(Node->Remaining, Jadeite);
		}
		if (Node->Remaining > 0)
		{
			Node->bDepletionLogged = false;
		}
		return;
	}

	// The pile is already gone. Put the carried ints in the bank so the trip is not deleted.
	FKodResourceCost Gain;
	Gain.Jadeite = Jadeite;
	Gain.Luminene = Luminene;
	Deposit(TeamId, Gain);
}

void UKodSimSubsystem::AbortGather(FKodSimEntityState& State)
{
	RefundGatherCargo(State);
	ClearGatherFields(State);
	State.AttackTarget = FKodEntityId();
	QuantizePose(State);
}

void UKodSimSubsystem::BeginGatherLeg(FKodSimEntityState& State, EKodGatherPhase Phase)
{
	State.GatherPhase = Phase;
	State.Order = EKodSimOrderType::Gather;
	State.GatherStallTicks = 0;
	State.bGatherStallRetargeted = false;
	State.AttackTarget = FKodEntityId();
	State.RetaliateTarget = FKodEntityId();
}

bool UKodSimSubsystem::RetargetGatherNode(FKodSimEntityState& State)
{
	const EKodResourceType Type = State.GatherType;
	FKodEntityId Next = FindNearestResourceNodeOfType(State.Position, Type, /*bRequireRemaining*/ true);
	if (!Next.IsValid())
	{
		Next = FindNearestResourceNodeOfType(State.Position, Type, /*bRequireRemaining*/ false);
	}
	if (!Next.IsValid())
	{
		State.GatherNode = FKodEntityId();
		return false;
	}
	State.GatherNode = Next;
	if (const FKodResourceNodeState* Node = ResourceNodes.Find(Next.Value))
	{
		State.GatherType = Node->Type;
	}
	return true;
}

bool UKodSimSubsystem::AdvanceGatherMove(FKodSimEntityState& State, const FVector& Target, float AcceptUU, float FixedDt)
{
	const float Accept = FMath::Max(1.f, AcceptUU);
	FVector ToTarget = Target - State.Position;
	ToTarget.Z = 0.f;
	const float Dist = ToTarget.Size();
	if (Dist <= Accept)
	{
		State.GatherStallTicks = 0;
		State.bGatherStallRetargeted = false;
		FaceStateToward(State, Target);
		QuantizePose(State);
		return true;
	}

	auto NoteStall = [&State](float MovedSq)
	{
		if (MovedSq < 0.25f)
		{
			++State.GatherStallTicks;
		}
		else
		{
			State.GatherStallTicks = 0;
		}
	};

	if (!State.bMobile || State.MoveSpeed <= 0.f)
	{
		NoteStall(0.f);
		return false;
	}

	const FVector Before = State.Position;
	const FVector Dir = ToTarget / Dist;
	State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	const float Step = FMath::Max(0.f, State.MoveSpeed) * FixedDt;
	// Land on the stand ring. Do not snap onto the node or building origin.
	const float Travel = FMath::Min(Step, FMath::Max(0.f, Dist - Accept));
	if (Travel <= KINDA_SMALL_NUMBER)
	{
		NoteStall(0.f);
		QuantizePose(State);
		return true;
	}
	State.Position += Dir * Travel;
	NoteStall(FVector::DistSquared2D(Before, State.Position));
	return false;
}

bool UKodSimSubsystem::RecoverGatherStall(FKodSimEntityState& State, float Dist, float AcceptUU)
{
	if (State.GatherStallTicks < KodEconomyDefaults::GatherStallTicks)
	{
		return false;
	}
	if (Dist <= AcceptUU * 1.25f)
	{
		State.GatherStallTicks = 0;
		State.bGatherStallRetargeted = false;
		QuantizePose(State);
		return true;
	}
	if (!State.bGatherStallRetargeted)
	{
		State.bGatherStallRetargeted = true;
		State.GatherStallTicks = 0;
		if (State.GatherPhase == EKodGatherPhase::ToDropOff)
		{
			State.GatherDropOff = FindNearestDropOff(State.TeamId, State.Position);
		}
		else
		{
			RetargetGatherNode(State);
		}
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("KodEcon Gather Abort Worker=%d Phase=%d"),
		State.Id.Value,
		static_cast<int32>(State.GatherPhase));
	if (State.IsCarryingCargo())
	{
		FKodResourceCost Gain;
		Gain.Jadeite = State.CargoJadeite;
		Gain.Luminene = State.CargoLuminene;
		State.CargoJadeite = 0;
		State.CargoLuminene = 0;
		State.bCargoFromTrickle = false;
		if (Gain.Jadeite > 0 || Gain.Luminene > 0)
		{
			Deposit(State.TeamId, Gain);
		}
	}
	ClearGatherFields(State);
	QuantizePose(State);
	return false;
}

FKodNodeTakeResult UKodSimSubsystem::TakeFromNode(FKodEntityId Id, int32 Requested)
{
	FKodNodeTakeResult Result;
	FKodResourceNodeState* Node = ResourceNodes.Find(Id.Value);
	if (!Node || Requested <= 0)
	{
		return Result;
	}

	Result.bFound = true;
	Result.Type = Node->Type;
	const int32 IdValue = Id.Value;

	if (Node->Remaining > 0)
	{
		Result.Amount = FMath::Min(Requested, Node->Remaining);
		Node->Remaining -= Result.Amount;
		const int32 Remaining = Node->Remaining;
		const EKodResourceType Type = Node->Type;
		const bool bDestroy = Remaining <= 0 && Node->TricklePerTrip <= 0;
		if (Remaining <= 0 && !Node->bDepletionLogged)
		{
			Node->bDepletionLogged = true;
			Result.bDepleted = true;
			UE_LOG(LogTemp, Log, TEXT("KodEcon NodeDepleted Id=%d"), IdValue);
		}
		UE_LOG(LogTemp, Log, TEXT("KodEcon Node Id=%d Type=%s Remaining=%d"),
			IdValue,
			KodResourceTypeName(Type),
			Remaining);
		if (bDestroy)
		{
			DestroyResourceNodeActor(Id);
		}
		return Result;
	}

	if (Node->TricklePerTrip > 0)
	{
		Result.Amount = Node->TricklePerTrip;
		Result.bFromTrickle = true;
		return Result;
	}

	Result.bFound = false;
	return Result;
}

void UKodSimSubsystem::StepEntityGather(FKodSimEntityState& State, float FixedDt)
{
	if (!State.bCanGather || State.Health <= 0.f)
	{
		AbortGather(State);
		return;
	}

	State.Order = EKodSimOrderType::Gather;

	if (State.GatherPhase == EKodGatherPhase::ToNode)
	{
		const FKodResourceNodeState* Node = ResourceNodes.Find(State.GatherNode.Value);
		const bool bUsable = Node && (Node->Remaining > 0 || Node->TricklePerTrip > 0);
		if (!bUsable)
		{
			if (!RetargetGatherNode(State))
			{
				if (State.IsCarryingCargo())
				{
					BeginGatherLeg(State, EKodGatherPhase::ToDropOff);
				}
				else
				{
					AbortGather(State);
				}
				return;
			}
			Node = ResourceNodes.Find(State.GatherNode.Value);
			if (!Node)
			{
				AbortGather(State);
				return;
			}
		}

		const float Stand = static_cast<float>(FMath::Max(1, Node->GatherStandUU));
		const float Dist = FVector::Dist2D(State.Position, Node->Position);
		if (Dist > Stand && State.GatherStallTicks >= KodEconomyDefaults::GatherStallTicks)
		{
			if (RecoverGatherStall(State, Dist, Stand))
			{
				BeginGatherLeg(State, EKodGatherPhase::Harvest);
				State.HarvestTicksRemaining = FMath::Max(1, Node->HarvestTicks);
			}
			return;
		}
		if (AdvanceGatherMove(State, Node->Position, Stand, FixedDt))
		{
			BeginGatherLeg(State, EKodGatherPhase::Harvest);
			if (const FKodResourceNodeState* Arrived = ResourceNodes.Find(State.GatherNode.Value))
			{
				State.HarvestTicksRemaining = FMath::Max(1, Arrived->HarvestTicks);
			}
			else
			{
				State.HarvestTicksRemaining = KodEconomyDefaults::JadeiteHarvestTicks;
			}
		}
		return;
	}

	if (State.GatherPhase == EKodGatherPhase::Harvest)
	{
		const FKodResourceNodeState* Node = ResourceNodes.Find(State.GatherNode.Value);
		if (!Node || (Node->Remaining <= 0 && Node->TricklePerTrip <= 0))
		{
			if (State.IsCarryingCargo())
			{
				BeginGatherLeg(State, EKodGatherPhase::ToDropOff);
			}
			else if (!RetargetGatherNode(State))
			{
				AbortGather(State);
			}
			else
			{
				BeginGatherLeg(State, EKodGatherPhase::ToNode);
			}
			return;
		}

		FaceStateToward(State, Node->Position);
		QuantizePose(State);
		State.HarvestTicksRemaining = FMath::Max(0, State.HarvestTicksRemaining - 1);
		if (State.HarvestTicksRemaining > 0)
		{
			return;
		}

		const int32 Requested = FMath::Max(1, Node->HarvestPerTrip);
		const FKodEntityId NodeId = State.GatherNode;
		const FKodNodeTakeResult Taken = TakeFromNode(NodeId, Requested);
		if (Taken.Amount > 0)
		{
			if (Taken.Type == EKodResourceType::Luminene)
			{
				State.CargoLuminene = Taken.Amount;
				State.CargoJadeite = 0;
			}
			else
			{
				State.CargoJadeite = Taken.Amount;
				State.CargoLuminene = 0;
			}
			State.bCargoFromTrickle = Taken.bFromTrickle;
			State.GatherType = Taken.Type;
			UE_LOG(LogTemp, Log, TEXT("KodEcon Gather Worker=%d Node=%d Amt=%d"),
				State.Id.Value,
				NodeId.Value,
				Taken.Amount);
		}

		if (Taken.bDepleted || !ResourceNodes.Contains(NodeId.Value))
		{
			const FKodEntityId Richer = FindNearestResourceNodeOfType(State.Position, State.GatherType, /*bRequireRemaining*/ true);
			if (Richer.IsValid())
			{
				State.GatherNode = Richer;
			}
			else if (!ResourceNodes.Contains(NodeId.Value) || !RetargetGatherNode(State))
			{
				State.GatherNode = FKodEntityId();
			}
		}
		else if (const FKodResourceNodeState* After = ResourceNodes.Find(NodeId.Value))
		{
			if (After->Remaining <= 0)
			{
				const FKodEntityId Richer = FindNearestResourceNodeOfType(State.Position, After->Type, /*bRequireRemaining*/ true);
				if (Richer.IsValid())
				{
					State.GatherNode = Richer;
				}
			}
		}

		if (State.IsCarryingCargo())
		{
			BeginGatherLeg(State, EKodGatherPhase::ToDropOff);
		}
		else if (State.GatherNode.IsValid() && ResourceNodes.Contains(State.GatherNode.Value))
		{
			BeginGatherLeg(State, EKodGatherPhase::ToNode);
		}
		else
		{
			AbortGather(State);
		}
		return;
	}

	if (State.GatherPhase == EKodGatherPhase::ToDropOff)
	{
		if (!State.IsCarryingCargo())
		{
			if (State.GatherNode.IsValid() && ResourceNodes.Contains(State.GatherNode.Value))
			{
				BeginGatherLeg(State, EKodGatherPhase::ToNode);
			}
			else if (!RetargetGatherNode(State))
			{
				AbortGather(State);
			}
			else
			{
				BeginGatherLeg(State, EKodGatherPhase::ToNode);
			}
			return;
		}

		if (!IsDropOff(State.GatherDropOff))
		{
			State.GatherDropOff = FindNearestDropOff(State.TeamId, State.Position);
		}
		const FKodDropOffState* Drop = DropOffs.Find(State.GatherDropOff.Value);
		if (!Drop)
		{
			FKodResourceCost Gain;
			Gain.Jadeite = State.CargoJadeite;
			Gain.Luminene = State.CargoLuminene;
			State.CargoJadeite = 0;
			State.CargoLuminene = 0;
			State.bCargoFromTrickle = false;
			Deposit(State.TeamId, Gain);
			if (State.GatherNode.IsValid() && ResourceNodes.Contains(State.GatherNode.Value))
			{
				BeginGatherLeg(State, EKodGatherPhase::ToNode);
			}
			else if (RetargetGatherNode(State))
			{
				BeginGatherLeg(State, EKodGatherPhase::ToNode);
			}
			else
			{
				ClearGatherFields(State);
				QuantizePose(State);
			}
			return;
		}

		const float Stand = static_cast<float>(FMath::Max(1, Drop->StandUU));
		const float Dist = FVector::Dist2D(State.Position, Drop->Position);
		if (Dist > Stand && State.GatherStallTicks >= KodEconomyDefaults::GatherStallTicks)
		{
			if (!RecoverGatherStall(State, Dist, Stand))
			{
				return;
			}
		}
		else if (!AdvanceGatherMove(State, Drop->Position, Stand, FixedDt))
		{
			return;
		}

		FKodResourceCost Gain;
		Gain.Jadeite = State.CargoJadeite;
		Gain.Luminene = State.CargoLuminene;
		State.CargoJadeite = 0;
		State.CargoLuminene = 0;
		State.bCargoFromTrickle = false;
		if (Gain.Jadeite > 0 || Gain.Luminene > 0)
		{
			Deposit(State.TeamId, Gain);
		}

		if (!State.GatherNode.IsValid() || !ResourceNodes.Contains(State.GatherNode.Value))
		{
			if (!RetargetGatherNode(State))
			{
				ClearGatherFields(State);
				QuantizePose(State);
				return;
			}
		}
		else if (const FKodResourceNodeState* Next = ResourceNodes.Find(State.GatherNode.Value))
		{
			if (Next->Remaining <= 0 && Next->TricklePerTrip <= 0)
			{
				if (!RetargetGatherNode(State))
				{
					ClearGatherFields(State);
					QuantizePose(State);
					return;
				}
			}
			else if (Next->Remaining <= 0)
			{
				const FKodEntityId Richer = FindNearestResourceNodeOfType(State.Position, Next->Type, /*bRequireRemaining*/ true);
				if (Richer.IsValid())
				{
					State.GatherNode = Richer;
				}
			}
		}
		BeginGatherLeg(State, EKodGatherPhase::ToNode);
		return;
	}

	AbortGather(State);
}

bool UKodSimSubsystem::IssueGather(FKodEntityId WorkerId, FKodEntityId NodeId)
{
	FKodSimEntityState* State = States.Find(WorkerId.Value);
	const FKodResourceNodeState* Node = ResourceNodes.Find(NodeId.Value);
	if (!State || !State->bCanGather || State->Health <= 0.f || !Node)
	{
		return false;
	}
	if (Node->Remaining <= 0 && Node->TricklePerTrip <= 0)
	{
		return false;
	}

	State->GatherType = Node->Type;
	if (Node->Remaining <= 0)
	{
		const FKodEntityId Richer = FindNearestResourceNodeOfType(State->Position, Node->Type, /*bRequireRemaining*/ true);
		if (Richer.IsValid())
		{
			NodeId = Richer;
			Node = ResourceNodes.Find(NodeId.Value);
		}
	}
	if (!Node)
	{
		return false;
	}

	State->GatherNode = NodeId;
	State->GatherType = Node->Type;
	State->GatherDropOff = FindNearestDropOff(State->TeamId, State->Position);
	if (State->IsCarryingCargo())
	{
		BeginGatherLeg(*State, EKodGatherPhase::ToDropOff);
	}
	else
	{
		State->bCargoFromTrickle = false;
		BeginGatherLeg(*State, EKodGatherPhase::ToNode);
	}
	FaceStateToward(*State, Node->Position);
	UE_LOG(LogTemp, Log, TEXT("KodEcon GatherOrder Worker=%d Node=%d Type=%s"),
		WorkerId.Value,
		NodeId.Value,
		KodResourceTypeName(State->GatherType));
	return true;
}

int32 UKodSimSubsystem::HarvestNode(FKodEntityId Id, int32 Amount, int32 TeamId)
{
	const FKodNodeTakeResult Taken = TakeFromNode(Id, Amount);
	if (!Taken.bFound || Taken.Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodEcon Harvest Miss Id=%d"), Id.Value);
		return 0;
	}

	FKodResourceCost Gain;
	if (Taken.Type == EKodResourceType::Luminene)
	{
		Gain.Luminene = Taken.Amount;
	}
	else
	{
		Gain.Jadeite = Taken.Amount;
	}
	Deposit(TeamId, Gain);
	return Taken.Amount;
}

void UKodSimSubsystem::CopyEntitySnapshot(TArray<FKodSimEntityState>& OutStates, TArray<AActor*>& OutActors) const
{
	OutStates.Reset();
	OutActors.Reset();

	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	OutStates.Reserve(Keys.Num());
	OutActors.Reserve(Keys.Num());

	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		const TWeakObjectPtr<AActor>* ActorPtr = Entities.Find(Key);
		AActor* Actor = ActorPtr ? ActorPtr->Get() : nullptr;
		if (!State || !Actor)
		{
			continue;
		}
		OutStates.Add(*State);
		OutActors.Add(Actor);
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
	// TeamId on combat entities is omitted so that prefix stays ids / pose / HP / order / gather.
	// Banks, resource nodes, and drop-offs are appended after that prefix as integers.
	// Visual bob, actor yaw, cargo mesh, and the resource readout are not appended.
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

		const uint8 GatherPhase = static_cast<uint8>(S.GatherPhase);
		Append(&GatherPhase, sizeof(GatherPhase));
		Append(&S.GatherNode.Value, sizeof(S.GatherNode.Value));
		Append(&S.GatherDropOff.Value, sizeof(S.GatherDropOff.Value));
		const uint8 GatherType = static_cast<uint8>(S.GatherType);
		Append(&GatherType, sizeof(GatherType));
		Append(&S.CargoJadeite, sizeof(S.CargoJadeite));
		Append(&S.CargoLuminene, sizeof(S.CargoLuminene));
		const uint8 TrickleCargo = S.bCargoFromTrickle ? 1 : 0;
		Append(&TrickleCargo, sizeof(TrickleCargo));
		Append(&S.HarvestTicksRemaining, sizeof(S.HarvestTicksRemaining));
		Append(&S.GatherStallTicks, sizeof(S.GatherStallTicks));
		const uint8 StallRetarget = S.bGatherStallRetargeted ? 1 : 0;
		Append(&StallRetarget, sizeof(StallRetarget));
	}

	// Economy is integers only: team id + bank, then node id + type + remaining + quantized position,
	// then drop-off id + team + quantized position.
	TArray<int32> TeamKeys;
	Banks.GetKeys(TeamKeys);
	TeamKeys.Sort();
	const int32 BankCount = TeamKeys.Num();
	Append(&BankCount, sizeof(BankCount));
	for (int32 TeamId : TeamKeys)
	{
		const FKodResourceCost& Bank = Banks.FindChecked(TeamId);
		Append(&TeamId, sizeof(TeamId));
		Append(&Bank.Jadeite, sizeof(Bank.Jadeite));
		Append(&Bank.Luminene, sizeof(Bank.Luminene));
	}

	TArray<int32> NodeKeys;
	ResourceNodes.GetKeys(NodeKeys);
	NodeKeys.Sort();
	const int32 NodeCount = NodeKeys.Num();
	Append(&NodeCount, sizeof(NodeCount));
	const float NodeQ = FMath::Max(KINDA_SMALL_NUMBER, PoseQuantizeUU);
	for (int32 NodeKey : NodeKeys)
	{
		const FKodResourceNodeState& Node = ResourceNodes.FindChecked(NodeKey);
		Append(&NodeKey, sizeof(NodeKey));
		const uint8 TypeByte = static_cast<uint8>(Node.Type);
		Append(&TypeByte, sizeof(TypeByte));
		Append(&Node.Remaining, sizeof(Node.Remaining));
		const int32 QX = FMath::RoundToInt(Node.Position.X / NodeQ);
		const int32 QY = FMath::RoundToInt(Node.Position.Y / NodeQ);
		const int32 QZ = FMath::RoundToInt(Node.Position.Z / NodeQ);
		Append(&QX, sizeof(QX));
		Append(&QY, sizeof(QY));
		Append(&QZ, sizeof(QZ));
	}

	TArray<int32> DropKeys;
	DropOffs.GetKeys(DropKeys);
	DropKeys.Sort();
	const int32 DropCount = DropKeys.Num();
	Append(&DropCount, sizeof(DropCount));
	for (int32 DropKey : DropKeys)
	{
		const FKodDropOffState& Drop = DropOffs.FindChecked(DropKey);
		Append(&DropKey, sizeof(DropKey));
		Append(&Drop.TeamId, sizeof(Drop.TeamId));
		const int32 QX = FMath::RoundToInt(Drop.Position.X / NodeQ);
		const int32 QY = FMath::RoundToInt(Drop.Position.Y / NodeQ);
		const int32 QZ = FMath::RoundToInt(Drop.Position.Z / NodeQ);
		Append(&QX, sizeof(QX));
		Append(&QY, sizeof(QY));
		Append(&QZ, sizeof(QZ));
	}

	const uint64 Hash = Bytes.Num() > 0
		? CityHash64(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num())
		: 0ull;
	return FString::Printf(TEXT("%016llX"), Hash);
}
