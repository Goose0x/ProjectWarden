#include "Sim/KodSimSubsystem.h"
#include "Game/KodPlayerController.h"
#include "Game/KodPlayerState.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Hash/CityHash.h"
#include "Misc/Crc.h"

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
	Supply.Reset();
	Structures.Reset();
	TrainSpecs.Reset();
	BuildSpecs.Reset();
	AbilitySpecs.Reset();
	UpgradeSpecs.Reset();
	TeamTech.Reset();
	TeamFacingYaw.Reset();
	UnitAbilities.Reset();
	PendingSpawns.Reset();
	PendingBuildingSpawns.Reset();
	bHasPlayable = false;
	TimeAccumulator = 0.f;
	SimTickIndex = 0;
	bAiEnabled = true;
	AiDifficulty = 1;
	bAiSeeded = false;
	bAiScoutSent = false;
	bHasEnemyStart = false;
	EnemyStartX = 0;
	EnemyStartY = 0;
	AiWavesLaunched = 0;
	AiNextWaveTick = 0;
	MatchOutcome = EKodMatchOutcome::None;
	bTeam0HadStructure = false;
	bTeam1HadStructure = false;
	AcquireGrid.Reset();
	StoredPaths.Reset();
	TeamVision.Reset();
	SeenStructures.Reset();
	SeenUnits.Reset();
	bAiCheatVision = false;
	VisionOriginX = 0;
	VisionOriginY = 0;
	VisionWidth = 0;
	VisionHeight = 0;
	NextPathId = 1;
	SimHz = KodBuildTicks::SimHz;
	MaxCatchUpSteps = KodBuildTicks::MaxCatchUpSteps;
	SetTeamBank(0, StartingJadeite, StartingLuminene);
}

void UKodSimSubsystem::Deinitialize()
{
	OnUnitFired.Clear();
	OnUnitHit.Clear();
	OnUnitKilled.Clear();
	OnTrainCompleted.Clear();
	OnCue.Clear();
	Entities.Reset();
	States.Reset();
	Banks.Reset();
	ResourceNodes.Reset();
	DropOffs.Reset();
	Supply.Reset();
	Structures.Reset();
	TrainSpecs.Reset();
	BuildSpecs.Reset();
	AbilitySpecs.Reset();
	UpgradeSpecs.Reset();
	TeamTech.Reset();
	TeamFacingYaw.Reset();
	UnitAbilities.Reset();
	PendingSpawns.Reset();
	PendingBuildingSpawns.Reset();
	AcquireGrid.Reset();
	StoredPaths.Reset();
	TeamVision.Reset();
	SeenStructures.Reset();
	SeenUnits.Reset();
	bAiCheatVision = false;
	VisionOriginX = 0;
	VisionOriginY = 0;
	VisionWidth = 0;
	VisionHeight = 0;
	bHasPlayable = false;
	PrevPositions.Reset();
	PrevYaws.Reset();
	bAiEnabled = true;
	AiDifficulty = 1;
	bAiSeeded = false;
	bAiScoutSent = false;
	bHasEnemyStart = false;
	EnemyStartX = 0;
	EnemyStartY = 0;
	AiWavesLaunched = 0;
	AiNextWaveTick = 0;
	MatchOutcome = EKodMatchOutcome::None;
	bTeam0HadStructure = false;
	bTeam1HadStructure = false;
	Super::Deinitialize();
}

TStatId UKodSimSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UKodSimSubsystem, STATGROUP_Tickables);
}

void UKodSimSubsystem::Tick(float DeltaTime)
{
	if (bSimPaused)
	{
		SyncActorPresentation(1.f);
		return;
	}
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
	RebuildAcquireGrid();
	// Stamp before orders so the first tick can see units that just spawned.
	// The stamp after separation matches the poses presentation hides against.
	StepVision();

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
		if (State->CooldownTicksRemaining > 0)
		{
			--State->CooldownTicksRemaining;
			State->CooldownRemaining = static_cast<float>(State->CooldownTicksRemaining) * FixedDt;
		}
		else
		{
			State->CooldownRemaining = 0.f;
		}
		if (State->bMobile
			&& State->Order == EKodSimOrderType::None
			&& State->GatherPhase == EKodGatherPhase::None
			&& IsWorldCellBlocked(State->Position))
		{
			SnapOutOfFootprint(*State);
			QuantizePose(*State);
		}

		if (State->InsideBunker.IsValid())
		{
			continue;
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
		case EKodSimOrderType::AttackMove:
			StepAttackMove(*State, FixedDt);
			break;
		case EKodSimOrderType::Hold:
			StepHold(*State, FixedDt);
			break;
		case EKodSimOrderType::Patrol:
			StepPatrol(*State, FixedDt);
			break;
		case EKodSimOrderType::Follow:
			StepFollow(*State, FixedDt, false);
			break;
		case EKodSimOrderType::Guard:
			StepFollow(*State, FixedDt, true);
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
	StepSeparation();
	StepVision();
	StepProduction();
	StepConstruction();
	StepAbilities();
	EvaluateMatch();
	StepAI();
	StepTech();
	StepExposed();
	RecordMatchSample();
}

void UKodSimSubsystem::StepEntityMove(FKodSimEntityState& State, float FixedDt)
{
	if (!State.bMobile || State.MoveSpeed <= 0.f)
	{
		State.Order = EKodSimOrderType::None;
		QuantizePose(State);
		return;
	}

	const bool bFollowPath = State.PathId != 0;
	const FVector Goal = GetSteeredGoal(State);
	const bool bPathDone = State.PathId == 0;
	const FVector ToTarget = Goal - State.Position;
	const float DistSq = ToTarget.SizeSquared2D();
	const float Accept = FMath::Max(1.f, State.AcceptanceRadius * 0.5f);
	if (!bFollowPath || bPathDone)
	{
		if (DistSq <= FMath::Square(Accept))
		{
			State.Position.X = State.MoveTarget.X;
			State.Position.Y = State.MoveTarget.Y;
			if (State.Order == EKodSimOrderType::Patrol)
			{
				const FVector Next = State.PatrolAnchor;
				State.PatrolAnchor = State.MoveTarget;
				State.MoveTarget = Next;
				State.PathId = PlanMovePath(State.Position, State.MoveTarget);
				State.PathCursor = 0;
				QuantizePose(State);
				return;
			}
			if (State.Order == EKodSimOrderType::Move)
			{
				CompleteCurrentOrder(State, FixedDt);
				return;
			}
			QuantizePose(State);
			return;
		}
	}
	else if (DistSq <= FMath::Square(Accept))
	{
		QuantizePose(State);
		return;
	}

	FVector Dir = ToTarget;
	Dir.Z = 0.f;
	Dir.Normalize();
	FVector Delta = SteerAroundBuildings(State, Dir * (EffectiveMoveSpeed(State) * FixedDt));
	if (Delta.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
		State.Position += Delta;
	}
	else
	{
		State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	}
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
	if (Target->TeamId != State.TeamId && !CanTeamSeeWorld(State.TeamId, Target->Position.X, Target->Position.Y))
	{
		State.BackswingTicksRemaining = 0;
		if (State.bExplicitAttack && State.bMobile)
		{
			FVector Dest = State.MoveTarget;
			FVector Seen;
			if (TryGetLastSeenPoint(State.TeamId, Target->Id.Value, Seen))
			{
				Dest = Seen;
			}
			Dest.Z = State.Position.Z;
			const int32 SelfId = State.Id.Value;
			TArray<FKodQueuedOrder> Rest = State.OrderQueue;
			IssueOrder(State.Id, EKodSimOrderType::AttackMove, Dest, FKodEntityId(), false);
			if (FKodSimEntityState* Live = States.Find(SelfId))
			{
				Live->OrderQueue = Rest;
				Live->bExplicitAttack = false;
			}
			UE_LOG(LogTemp, Log, TEXT("KodOrder FogChase Unit=%d Dest=%.0f,%.0f"), SelfId, Dest.X, Dest.Y);
			return;
		}
		State.AttackTarget = FKodEntityId();
		State.bExplicitAttack = false;
		if (State.Order == EKodSimOrderType::Attack)
		{
			CompleteCurrentOrder(State, FixedDt);
		}
		else
		{
			QuantizePose(State);
		}
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

	if (State.WeaponDamage > 0.f && TryApplyWeapon(State, *Target))
	{
		return;
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

namespace
{
	constexpr int32 AcquireCellUU = 1024;

	int64 PackAcquireCell(int32 X, int32 Y)
	{
		return (static_cast<int64>(static_cast<uint32>(X)) << 32) | static_cast<uint32>(Y);
	}
}

void UKodSimSubsystem::RebuildAcquireGrid()
{
	AcquireGrid.Reset();
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->Health <= 0.f)
		{
			continue;
		}
		const int32 CellX = FMath::FloorToInt(State->Position.X / static_cast<float>(AcquireCellUU));
		const int32 CellY = FMath::FloorToInt(State->Position.Y / static_cast<float>(AcquireCellUU));
		AcquireGrid.FindOrAdd(PackAcquireCell(CellX, CellY)).Add(Key);
	}
}

void UKodSimSubsystem::CollectAcquireCandidates(float OriginX, float OriginY, float Range, TArray<int32>& OutIds) const
{
	OutIds.Reset();
	const float SafeRange = FMath::Max(0.f, Range);
	const int32 MinX = FMath::FloorToInt((OriginX - SafeRange) / static_cast<float>(AcquireCellUU));
	const int32 MaxX = FMath::FloorToInt((OriginX + SafeRange) / static_cast<float>(AcquireCellUU));
	const int32 MinY = FMath::FloorToInt((OriginY - SafeRange) / static_cast<float>(AcquireCellUU));
	const int32 MaxY = FMath::FloorToInt((OriginY + SafeRange) / static_cast<float>(AcquireCellUU));
	for (int32 X = MinX; X <= MaxX; ++X)
	{
		for (int32 Y = MinY; Y <= MaxY; ++Y)
		{
			if (const TArray<int32>* Cell = AcquireGrid.Find(PackAcquireCell(X, Y)))
			{
				OutIds.Append(*Cell);
			}
		}
	}
	OutIds.Sort();
}

FKodEntityId UKodSimSubsystem::FindNearestEnemyInRange(const FKodSimEntityState& State) const
{
	// Resource nodes are not in States, so they are not enemies and do not auto-acquire.
	FKodEntityId BestId;
	float BestDistSq = 0.f;
	bool bFound = false;
	const float Range = FMath::Max(0.f, State.WeaponRange);
	const float RangeSq = FMath::Square(Range);

	TArray<int32> Keys;
	CollectAcquireCandidates(State.Position.X, State.Position.Y, Range, Keys);
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
		// Structures stay out of idle acquire. An explicit attack still damages them.
		if (!Other->bMobile)
		{
			continue;
		}
		// Unarmed workers stay out of the marine duel. RMB Attack can still target them.
		if (Other->bCanGather && Other->WeaponDamage <= 0.f)
		{
			continue;
		}
		if (!CanTeamSeeWorld(State.TeamId, Other->Position.X, Other->Position.Y))
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

FKodEntityId UKodSimSubsystem::FindNearestEnemyStructureInRange(const FKodSimEntityState& State) const
{
	FKodEntityId BestId;
	float BestDistSq = 0.f;
	bool bFound = false;
	const float Range = FMath::Max(0.f, State.WeaponRange);
	const float RangeSq = FMath::Square(Range);

	TArray<int32> Keys;
	CollectAcquireCandidates(State.Position.X, State.Position.Y, Range, Keys);
	for (int32 Key : Keys)
	{
		if (Key == State.Id.Value)
		{
			continue;
		}
		const FKodSimEntityState* Other = States.Find(Key);
		if (!Other || Other->bMobile || Other->Health <= 0.f || Other->TeamId == State.TeamId)
		{
			continue;
		}
		if (!CanTeamSeeWorld(State.TeamId, Other->Position.X, Other->Position.Y))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared2D(State.Position, Other->Position);
		if (DistSq > RangeSq)
		{
			continue;
		}
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
			if (Aggro->Health > 0.f && Aggro->TeamId != State.TeamId && Aggro->Id.Value != State.Id.Value
				&& CanTeamSeeWorld(State.TeamId, Aggro->Position.X, Aggro->Position.Y))
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
	if (!Chosen.IsValid())
	{
		Chosen = FindNearestEnemyStructureInRange(State);
		if (Chosen.IsValid())
		{
			Reason = TEXT("Structure");
		}
	}

	if (!Chosen.IsValid() || !Reason)
	{
		return false;
	}

	State.Order = EKodSimOrderType::Attack;
	State.bExplicitAttack = false;
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
	int32 LocalTeam = 0;
	if (const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
	{
		if (const AKodPlayerState* PlayerState = PC->GetPlayerState<AKodPlayerState>())
		{
			LocalTeam = PlayerState->TeamId;
		}
	}

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

		if (Pair.Value.InsideBunker.IsValid())
		{
			Actor->SetActorHiddenInGame(true);
			continue;
		}
		if (Pair.Value.TeamId == LocalTeam)
		{
			Actor->SetActorHiddenInGame(false);
		}
		else
		{
			// Enemy units and structures are live only in current vision.
			// Explored cells keep a last-seen structure snapshot on the HUD.
			Actor->SetActorHiddenInGame(GetTeamVision(LocalTeam, Pair.Value.Position.X, Pair.Value.Position.Y) != 2);
		}
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
	ReleaseCountedSupply(Id);
	ReleaseStructureRecord(Id);
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
	bool bCanGather,
	int32 BodyRadiusUU,
	int32 SightRadiusUU)
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
	State->WeaponCooldownTicks = FMath::Max(0, FMath::RoundToInt(WeaponCooldownSeconds * static_cast<float>(KodBuildTicks::SimHz)));
	State->CooldownTicksRemaining = 0;
	State->CooldownRemaining = 0.f;
	State->TeamId = TeamId;
	State->bCanGather = bCanGather;
	State->BodyRadiusUU = FMath::Clamp(BodyRadiusUU, 8, 256);
	State->SightRadiusUU = FMath::Clamp(SightRadiusUU, 0, 10000);
	State->WeaponBackswingTicks = 2;
	ApplyRoster(Id);
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

void UKodSimSubsystem::StepAttackMove(FKodSimEntityState& State, float FixedDt)
{
	if (!State.bMobile || State.Health <= 0.f || State.MoveSpeed <= 0.f)
	{
		State.Order = EKodSimOrderType::None;
		QuantizePose(State);
		return;
	}

	const FVector Dest = State.MoveTarget;
	const int32 SelfId = State.Id.Value;
	FKodEntityId Threat = FindNearestEnemyInRange(State);
	if (!Threat.IsValid())
	{
		Threat = FindNearestEnemyStructureInRange(State);
	}
	if (Threat.IsValid())
	{
		State.AttackTarget = Threat;
		State.Order = EKodSimOrderType::Attack;
		StepEntityAttack(State, FixedDt);
		if (FKodSimEntityState* Live = States.Find(SelfId))
		{
			if (Live->Health > 0.f)
			{
				Live->Order = EKodSimOrderType::AttackMove;
				Live->MoveTarget = Dest;
			}
		}
		return;
	}

	State.AttackTarget = FKodEntityId();
	State.Order = EKodSimOrderType::AttackMove;
	State.MoveTarget = Dest;
	StepEntityMove(State, FixedDt);
}

bool UKodSimSubsystem::IsAttackable(FKodEntityId TargetId, int32 AttackerTeam) const
{
	if (!TargetId.IsValid() || IsResourceNode(TargetId))
	{
		return false;
	}
	const FKodSimEntityState* Target = States.Find(TargetId.Value);
	if (!Target || Target->Health <= 0.f)
	{
		return false;
	}
	if (IsDropOff(TargetId) && Target->TeamId == AttackerTeam)
	{
		return false;
	}
	return true;
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
	NoteResources(TeamId, FMath::Max(0, Amount.Jadeite) + FMath::Max(0, Amount.Luminene));
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
	if (Type == EKodResourceType::Luminene && Next.IsValid() && !FindCompletedRig(State.TeamId, Next).IsValid())
	{
		Next = FKodEntityId();
		TArray<int32> Keys;
		ResourceNodes.GetKeys(Keys);
		Keys.Sort();
		int64 BestDist = MAX_int64;
		for (int32 Key : Keys)
		{
			const FKodResourceNodeState* Node = ResourceNodes.Find(Key);
			if (!Node || Node->Type != EKodResourceType::Luminene)
			{
				continue;
			}
			if (!FindCompletedRig(State.TeamId, Node->Id).IsValid())
			{
				continue;
			}
			const int64 DX = FMath::RoundToInt(Node->Position.X - State.Position.X);
			const int64 DY = FMath::RoundToInt(Node->Position.Y - State.Position.Y);
			const int64 Dist = DX * DX + DY * DY;
			if (!Next.IsValid() || Dist < BestDist)
			{
				BestDist = Dist;
				Next.Value = Key;
			}
		}
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
	const float Step = FMath::Max(0.f, EffectiveMoveSpeed(State)) * FixedDt;
	// Land on the stand ring. Do not snap onto the node or building origin.
	const float Travel = FMath::Min(Step, FMath::Max(0.f, Dist - Accept));
	if (Travel <= KINDA_SMALL_NUMBER)
	{
		NoteStall(0.f);
		QuantizePose(State);
		return true;
	}
	FVector Delta = SteerAroundBuildings(State, Dir * Travel);
	State.Position += Delta;
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

		FVector Approach = Node->Position;
		if (!TryGetGatherApproach(State.TeamId, *Node, Approach))
		{
			if (!RetargetGatherNode(State))
			{
				AbortGather(State);
			}
			return;
		}
		const float Stand = static_cast<float>(FMath::Max(1, Node->GatherStandUU));
		const float Dist = FVector::Dist2D(State.Position, Approach);
		if (Dist > Stand && State.GatherStallTicks >= KodEconomyDefaults::GatherStallTicks)
		{
			if (RecoverGatherStall(State, Dist, Stand))
			{
				BeginGatherLeg(State, EKodGatherPhase::Harvest);
				State.HarvestTicksRemaining = FMath::Max(1, Node->HarvestTicks);
			}
			return;
		}
		if (AdvanceGatherMove(State, Approach, Stand, FixedDt))
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
	FVector Approach = Node->Position;
	if (!TryGetGatherApproach(State->TeamId, *Node, Approach))
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
	ClearBuildAssignment(*State);
	FaceStateToward(*State, Approach);
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

void UKodSimSubsystem::CopyResourceNodes(TArray<FKodResourceNodeState>& OutNodes) const
{
	OutNodes.Reset();
	TArray<int32> Keys;
	ResourceNodes.GetKeys(Keys);
	Keys.Sort();
	OutNodes.Reserve(Keys.Num());
	for (int32 Key : Keys)
	{
		if (const FKodResourceNodeState* Node = ResourceNodes.Find(Key))
		{
			OutNodes.Add(*Node);
		}
	}
}

void UKodSimSubsystem::CopyStructures(TArray<FKodStructureState>& OutStructures) const
{
	OutStructures.Reset();
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	OutStructures.Reserve(Keys.Num());
	for (int32 Key : Keys)
	{
		if (const FKodStructureState* Structure = Structures.Find(Key))
		{
			OutStructures.Add(*Structure);
		}
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
	// Banks, supply, resource nodes, drop-offs, and structure queues are appended after that prefix as integers.
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

		const FString DefinitionName = S.DefinitionId.ToString();
		const int32 DefHash = static_cast<int32>(FCrc::StrCrc32(*DefinitionName));
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
		const uint8 ExplicitAttack = S.bExplicitAttack ? 1 : 0;
		Append(&ExplicitAttack, sizeof(ExplicitAttack));

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
		Append(&S.SupplyCost, sizeof(S.SupplyCost));
		Append(&S.WeaponCooldownTicks, sizeof(S.WeaponCooldownTicks));
		Append(&S.CooldownTicksRemaining, sizeof(S.CooldownTicksRemaining));
		Append(&S.BodyRadiusUU, sizeof(S.BodyRadiusUU));
		Append(&S.SightRadiusUU, sizeof(S.SightRadiusUU));
		Append(&S.ArmorPoints, sizeof(S.ArmorPoints));
		Append(&S.Energy, sizeof(S.Energy));
		Append(&S.EnergyMax, sizeof(S.EnergyMax));
		Append(&S.EnergyRegenAcc, sizeof(S.EnergyRegenAcc));
		Append(&S.HealAcc, sizeof(S.HealAcc));
		const uint8 Autocast = S.bAutocastHeal ? 1 : 0;
		Append(&Autocast, sizeof(Autocast));
		const uint8 Biological = S.bBiological ? 1 : 0;
		Append(&Biological, sizeof(Biological));
		const uint8 Infantry = S.bInfantry ? 1 : 0;
		Append(&Infantry, sizeof(Infantry));
		const uint8 Healer = S.bHealer ? 1 : 0;
		Append(&Healer, sizeof(Healer));
		const uint8 Flyer = S.bFlyer ? 1 : 0;
		Append(&Flyer, sizeof(Flyer));
		Append(&S.CargoSize, sizeof(S.CargoSize));
		Append(&S.InsideBunker.Value, sizeof(S.InsideBunker.Value));
		Append(&S.LoadTarget.Value, sizeof(S.LoadTarget.Value));
		Append(&S.WeaponBackswingTicks, sizeof(S.WeaponBackswingTicks));
		Append(&S.BackswingTicksRemaining, sizeof(S.BackswingTicksRemaining));
		Append(&S.FollowTarget.Value, sizeof(S.FollowTarget.Value));
		Append(&S.PathId, sizeof(S.PathId));
		Append(&S.PathCursor, sizeof(S.PathCursor));
		Append(&S.PathOffsetX, sizeof(S.PathOffsetX));
		Append(&S.PathOffsetY, sizeof(S.PathOffsetY));
		const int32 PatrolX = FMath::RoundToInt(S.PatrolAnchor.X);
		const int32 PatrolY = FMath::RoundToInt(S.PatrolAnchor.Y);
		Append(&PatrolX, sizeof(PatrolX));
		Append(&PatrolY, sizeof(PatrolY));
		const int32 QueueCount = S.OrderQueue.Num();
		Append(&QueueCount, sizeof(QueueCount));
		for (const FKodQueuedOrder& Step : S.OrderQueue)
		{
			const uint8 Kind = static_cast<uint8>(Step.Order);
			Append(&Kind, sizeof(Kind));
			Append(&Step.X, sizeof(Step.X));
			Append(&Step.Y, sizeof(Step.Y));
			Append(&Step.TargetId, sizeof(Step.TargetId));
			Append(&Step.PathId, sizeof(Step.PathId));
			Append(&Step.OffsetX, sizeof(Step.OffsetX));
			Append(&Step.OffsetY, sizeof(Step.OffsetY));
		}
		const uint8 SupplyCounted = S.bSupplyCounted ? 1 : 0;
		Append(&SupplyCounted, sizeof(SupplyCounted));
		Append(&S.BuildSite.Value, sizeof(S.BuildSite.Value));
		const FString AbilityName = S.ActiveAbilityId.ToString();
		const int32 AbilityLen = AbilityName.Len();
		Append(&AbilityLen, sizeof(AbilityLen));
		if (AbilityLen > 0)
		{
			Append(*AbilityName, AbilityLen * static_cast<int32>(sizeof(TCHAR)));
		}
		Append(&S.AbilityTicksRemaining, sizeof(S.AbilityTicksRemaining));
		Append(&S.AbilityCooldownTicks, sizeof(S.AbilityCooldownTicks));
		Append(&S.AbilityMovePermille, sizeof(S.AbilityMovePermille));
		Append(&S.AbilityAttackPermille, sizeof(S.AbilityAttackPermille));
	}

	// Economy is integers only: team id + bank, then node id + type + remaining + quantized position,
	// then drop-off id + team + quantized position.
	TArray<int32> TeamKeys;
	Banks.GetKeys(TeamKeys);
	TArray<int32> SupplyKeys;
	Supply.GetKeys(SupplyKeys);
	for (int32 SupplyTeam : SupplyKeys)
	{
		TeamKeys.AddUnique(SupplyTeam);
	}
	TeamKeys.Sort();
	const int32 BankCount = TeamKeys.Num();
	Append(&BankCount, sizeof(BankCount));
	for (int32 TeamId : TeamKeys)
	{
		const FKodResourceCost* BankPtr = Banks.Find(TeamId);
		const FKodResourceCost Bank = BankPtr ? *BankPtr : FKodResourceCost();
		Append(&TeamId, sizeof(TeamId));
		Append(&Bank.Jadeite, sizeof(Bank.Jadeite));
		Append(&Bank.Luminene, sizeof(Bank.Luminene));
		const FKodSupplyState* SupplyRow = Supply.Find(TeamId);
		const int32 SupplyUsed = SupplyRow ? SupplyRow->Used : 0;
		const int32 SupplyCap = SupplyRow ? SupplyRow->Cap : 0;
		Append(&SupplyUsed, sizeof(SupplyUsed));
		Append(&SupplyCap, sizeof(SupplyCap));
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

	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	const int32 StructureCount = StructureKeys.Num();
	Append(&StructureCount, sizeof(StructureCount));
	for (int32 StructureKey : StructureKeys)
	{
		const FKodStructureState& Structure = Structures.FindChecked(StructureKey);
		Append(&StructureKey, sizeof(StructureKey));
		Append(&Structure.TeamId, sizeof(Structure.TeamId));
		Append(&Structure.SupplyProvided, sizeof(Structure.SupplyProvided));
		const int32 SlotCount = Structure.Slots.Num();
		Append(&SlotCount, sizeof(SlotCount));
		for (const FKodProductionSlot& Slot : Structure.Slots)
		{
			const FString UnitName = Slot.UnitId.ToString();
			const int32 NameLen = UnitName.Len();
			Append(&NameLen, sizeof(NameLen));
			if (NameLen > 0)
			{
				Append(*UnitName, NameLen * static_cast<int32>(sizeof(TCHAR)));
			}
			Append(&Slot.BuildTime, sizeof(Slot.BuildTime));
			const uint8 Started = Slot.bStarted ? 1 : 0;
			Append(&Started, sizeof(Started));
		}
		Append(&Structure.ActiveTicksRemaining, sizeof(Structure.ActiveTicksRemaining));
		Append(&Structure.CellX, sizeof(Structure.CellX));
		Append(&Structure.CellY, sizeof(Structure.CellY));
		Append(&Structure.FootprintX, sizeof(Structure.FootprintX));
		Append(&Structure.FootprintY, sizeof(Structure.FootprintY));
		Append(&Structure.BuildTimeTotal, sizeof(Structure.BuildTimeTotal));
		Append(&Structure.BuildTicksDone, sizeof(Structure.BuildTicksDone));
		const uint8 BuildComplete = Structure.bComplete ? 1 : 0;
		Append(&BuildComplete, sizeof(BuildComplete));
		Append(&Structure.BuilderId.Value, sizeof(Structure.BuilderId.Value));
		Append(&Structure.VentId.Value, sizeof(Structure.VentId.Value));
		const uint8 Blocked = Structure.bSupplyBlocked ? 1 : 0;
		Append(&Blocked, sizeof(Blocked));
		const int32 RallyNode = Structure.bRallyNode ? Structure.RallyNode.Value : 0;
		Append(&RallyNode, sizeof(RallyNode));
		Append(&Structure.RallyUnit.Value, sizeof(Structure.RallyUnit.Value));
		const uint8 Lowered = Structure.bLowered ? 1 : 0;
		Append(&Lowered, sizeof(Lowered));
		Append(&Structure.CargoCap, sizeof(Structure.CargoCap));
		TArray<int32> Cargo = Structure.Cargo;
		Cargo.Sort();
		const int32 CargoCount = Cargo.Num();
		Append(&CargoCount, sizeof(CargoCount));
		for (int32 Rider : Cargo)
		{
			Append(&Rider, sizeof(Rider));
		}
		const int32 ResearchCount = Structure.Research.Num();
		Append(&ResearchCount, sizeof(ResearchCount));
		Append(&Structure.ResearchTicksRemaining, sizeof(Structure.ResearchTicksRemaining));
		for (const FKodProductionSlot& Job : Structure.Research)
		{
			const FString UpgradeName = Job.UnitId.ToString();
			const int32 UpgradeCrc = static_cast<int32>(FCrc::StrCrc32(*UpgradeName));
			Append(&UpgradeCrc, sizeof(UpgradeCrc));
			Append(&Job.BuildTime, sizeof(Job.BuildTime));
		}
		const int32 RQX = FMath::RoundToInt(Structure.RallyPoint.X / NodeQ);
		const int32 RQY = FMath::RoundToInt(Structure.RallyPoint.Y / NodeQ);
		const int32 RQZ = FMath::RoundToInt(Structure.RallyPoint.Z / NodeQ);
		Append(&RQX, sizeof(RQX));
		Append(&RQY, sizeof(RQY));
		Append(&RQZ, sizeof(RQZ));
	}

	const uint8 Outcome = static_cast<uint8>(MatchOutcome);
	Append(&Outcome, sizeof(Outcome));
	const uint8 AiOn = bAiEnabled ? 1 : 0;
	Append(&AiOn, sizeof(AiOn));
	Append(&AiDifficulty, sizeof(AiDifficulty));
	Append(&AiWavesLaunched, sizeof(AiWavesLaunched));
	Append(&AiNextWaveTick, sizeof(AiNextWaveTick));
	const uint8 Seeded = bAiSeeded ? 1 : 0;
	Append(&Seeded, sizeof(Seeded));
	const uint8 ScoutSent = bAiScoutSent ? 1 : 0;
	Append(&ScoutSent, sizeof(ScoutSent));
	const uint8 HasEnemyStart = bHasEnemyStart ? 1 : 0;
	Append(&HasEnemyStart, sizeof(HasEnemyStart));
	Append(&EnemyStartX, sizeof(EnemyStartX));
	Append(&EnemyStartY, sizeof(EnemyStartY));
	TArray<int32> TechTeams;
	TeamTech.GetKeys(TechTeams);
	TechTeams.Sort();
	const int32 TechCount = TechTeams.Num();
	Append(&TechCount, sizeof(TechCount));
	for (int32 TeamId : TechTeams)
	{
		const FKodTeamTech& Tech = TeamTech.FindChecked(TeamId);
		Append(&TeamId, sizeof(TeamId));
		Append(&Tech.InfantryWeapons, sizeof(Tech.InfantryWeapons));
		Append(&Tech.InfantryArmor, sizeof(Tech.InfantryArmor));
	}
	TArray<int32> FacingTeams;
	TeamFacingYaw.GetKeys(FacingTeams);
	FacingTeams.Sort();
	const int32 FacingCount = FacingTeams.Num();
	Append(&FacingCount, sizeof(FacingCount));
	for (int32 TeamId : FacingTeams)
	{
		const int32 Yaw = TeamFacingYaw.FindChecked(TeamId);
		Append(&TeamId, sizeof(TeamId));
		Append(&Yaw, sizeof(Yaw));
	}
	const uint8 Cheat = bAiCheatVision ? 1 : 0;
	Append(&Cheat, sizeof(Cheat));

	Append(&VisionOriginX, sizeof(VisionOriginX));
	Append(&VisionOriginY, sizeof(VisionOriginY));
	Append(&VisionWidth, sizeof(VisionWidth));
	Append(&VisionHeight, sizeof(VisionHeight));
	TArray<int32> VisionTeams;
	TeamVision.GetKeys(VisionTeams);
	VisionTeams.Sort();
	const int32 VisionTeamCount = VisionTeams.Num();
	Append(&VisionTeamCount, sizeof(VisionTeamCount));
	for (int32 TeamId : VisionTeams)
	{
		Append(&TeamId, sizeof(TeamId));
		const TArray<uint8>& Grid = TeamVision.FindChecked(TeamId);
		const int32 CellCount = Grid.Num();
		Append(&CellCount, sizeof(CellCount));
		if (CellCount > 0)
		{
			Append(Grid.GetData(), CellCount);
		}
		const TArray<FKodSeenStructure>* Seen = SeenStructures.Find(TeamId);
		const int32 SeenCount = Seen ? Seen->Num() : 0;
		Append(&SeenCount, sizeof(SeenCount));
		if (Seen)
		{
			for (const FKodSeenStructure& Entry : *Seen)
			{
				Append(&Entry.Id, sizeof(Entry.Id));
				Append(&Entry.TeamId, sizeof(Entry.TeamId));
				const FString SeenName = Entry.DefinitionId.ToString();
				const int32 SeenDef = static_cast<int32>(FCrc::StrCrc32(*SeenName));
				Append(&SeenDef, sizeof(SeenDef));
				Append(&Entry.PosX, sizeof(Entry.PosX));
				Append(&Entry.PosY, sizeof(Entry.PosY));
				Append(&Entry.FootprintX, sizeof(Entry.FootprintX));
				Append(&Entry.FootprintY, sizeof(Entry.FootprintY));
				Append(&Entry.Health, sizeof(Entry.Health));
				Append(&Entry.MaxHealth, sizeof(Entry.MaxHealth));
			}
		}
		const TArray<FKodSeenUnit>* SeenMobile = SeenUnits.Find(TeamId);
		const int32 SeenUnitCount = SeenMobile ? SeenMobile->Num() : 0;
		Append(&SeenUnitCount, sizeof(SeenUnitCount));
		if (SeenMobile)
		{
			for (const FKodSeenUnit& Entry : *SeenMobile)
			{
				Append(&Entry.Id, sizeof(Entry.Id));
				Append(&Entry.TeamId, sizeof(Entry.TeamId));
				Append(&Entry.PosX, sizeof(Entry.PosX));
				Append(&Entry.PosY, sizeof(Entry.PosY));
			}
		}
	}

	const uint8 ExposedRuleByte = bExposedRule ? 1 : 0;
	Append(&ExposedRuleByte, sizeof(ExposedRuleByte));
	Append(&ExposedTicks, sizeof(ExposedTicks));
	const uint8 Ex0 = bTeam0Exposed ? 1 : 0;
	const uint8 Ex1 = bTeam1Exposed ? 1 : 0;
	const uint8 Warn0 = bTeam0ExposedWarned ? 1 : 0;
	const uint8 Warn1 = bTeam1ExposedWarned ? 1 : 0;
	Append(&Ex0, sizeof(Ex0));
	Append(&Ex1, sizeof(Ex1));
	Append(&Warn0, sizeof(Warn0));
	Append(&Warn1, sizeof(Warn1));
	auto AppendTotals = [&](const FKodTeamTotals& Totals)
	{
		Append(&Totals.Collected, sizeof(Totals.Collected));
		Append(&Totals.Trained, sizeof(Totals.Trained));
		Append(&Totals.Killed, sizeof(Totals.Killed));
		Append(&Totals.Built, sizeof(Totals.Built));
		Append(&Totals.Razed, sizeof(Totals.Razed));
	};
	AppendTotals(Totals0);
	AppendTotals(Totals1);
	auto AppendSamples = [&](const TArray<FKodStatSample>& Samples)
	{
		const int32 Count = Samples.Num();
		Append(&Count, sizeof(Count));
		for (const FKodStatSample& Sample : Samples)
		{
			Append(&Sample.Tick, sizeof(Sample.Tick));
			Append(&Sample.Collected, sizeof(Sample.Collected));
			Append(&Sample.Trained, sizeof(Sample.Trained));
			Append(&Sample.ArmyValue, sizeof(Sample.ArmyValue));
			Append(&Sample.Income, sizeof(Sample.Income));
			Append(&Sample.Killed, sizeof(Sample.Killed));
			Append(&Sample.Built, sizeof(Sample.Built));
			Append(&Sample.Razed, sizeof(Sample.Razed));
		}
	};
	AppendSamples(Samples0);
	AppendSamples(Samples1);

	const uint64 Hash = Bytes.Num() > 0
		? CityHash64(reinterpret_cast<const char*>(Bytes.GetData()), Bytes.Num())
		: 0ull;
	return FString::Printf(TEXT("%016llX"), Hash);
}
