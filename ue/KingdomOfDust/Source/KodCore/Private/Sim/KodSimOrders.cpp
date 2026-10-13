#include "Sim/KodSimSubsystem.h"

#include "Warden/KodWardenPaths.h"

#include "GameFramework/Actor.h"

namespace
{
	constexpr int32 OrderQueueCap = 8;

	const TCHAR* OrderLabel(EKodSimOrderType Kind)
	{
		switch (Kind)
		{
		case EKodSimOrderType::Move:
			return TEXT("Move");
		case EKodSimOrderType::Attack:
			return TEXT("Attack");
		case EKodSimOrderType::AttackMove:
			return TEXT("AttackMove");
		case EKodSimOrderType::Hold:
			return TEXT("Hold");
		case EKodSimOrderType::Patrol:
			return TEXT("Patrol");
		case EKodSimOrderType::Follow:
			return TEXT("Follow");
		case EKodSimOrderType::Guard:
			return TEXT("Guard");
		case EKodSimOrderType::Gather:
			return TEXT("Gather");
		default:
			return TEXT("None");
		}
	}
}

void UKodSimSubsystem::IssueMove(FKodEntityId Id, FVector WorldLocation)
{
	IssueOrder(Id, EKodSimOrderType::Move, WorldLocation, FKodEntityId(), false);
}

void UKodSimSubsystem::IssueAttackMove(FKodEntityId Id, FVector WorldLocation)
{
	IssueOrder(Id, EKodSimOrderType::AttackMove, WorldLocation, FKodEntityId(), false);
}

void UKodSimSubsystem::IssueAttack(FKodEntityId Id, FKodEntityId TargetId)
{
	IssueAttackForced(Id, TargetId, false);
}

void UKodSimSubsystem::IssueAttackForced(FKodEntityId Id, FKodEntityId TargetId, bool bForced)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State)
	{
		return;
	}
	if (!bForced && !IsAttackable(TargetId, State->TeamId))
	{
		return;
	}
	if (bForced)
	{
		if (IsResourceNode(TargetId))
		{
			return;
		}
		const FKodSimEntityState* Target = States.Find(TargetId.Value);
		if (!Target || Target->Health <= 0.f || Target->Id.Value == Id.Value)
		{
			return;
		}
	}
	const FKodSimEntityState* Target = States.Find(TargetId.Value);
	const FVector Point = Target ? Target->Position : State->Position;
	IssueOrder(Id, EKodSimOrderType::Attack, Point, TargetId, false);
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
		ClearBuildAssignment(*State);
		State->OrderQueue.Reset();
		State->Order = EKodSimOrderType::None;
		State->AttackTarget = FKodEntityId();
		State->FollowTarget = FKodEntityId();
		State->RetaliateTarget = FKodEntityId();
		State->bExplicitAttack = false;
		State->PathId = 0;
		State->PathCursor = 0;
		State->BackswingTicksRemaining = 0;
		QuantizePose(*State);
		UE_LOG(LogTemp, Verbose, TEXT("KodOrder Stop Unit=%d"), Id.Value);
	}
}

void UKodSimSubsystem::IssueOrder(
	FKodEntityId Id,
	EKodSimOrderType Kind,
	FVector Point,
	FKodEntityId Target,
	bool bQueued,
	int32 SharedPathId,
	int32 OffsetX,
	int32 OffsetY)
{
	FKodSimEntityState* State = States.Find(Id.Value);
	if (!State || !State->bMobile || State->Health <= 0.f || Kind == EKodSimOrderType::None)
	{
		return;
	}

	if (bQueued && State->Order != EKodSimOrderType::None)
	{
		if (State->OrderQueue.Num() >= OrderQueueCap)
		{
			UE_LOG(LogTemp, Verbose, TEXT("KodOrder QueueFull Unit=%d"), Id.Value);
			return;
		}
		FKodQueuedOrder Step;
		Step.Order = Kind;
		Step.X = FMath::RoundToInt(Point.X);
		Step.Y = FMath::RoundToInt(Point.Y);
		Step.TargetId = Target.Value;
		Step.PathId = SharedPathId;
		Step.OffsetX = OffsetX;
		Step.OffsetY = OffsetY;
		State->OrderQueue.Add(Step);
		UE_LOG(LogTemp, Verbose, TEXT("KodOrder Queue Unit=%d Kind=%s Target=%d"),
			Id.Value,
			OrderLabel(Kind),
			Target.Value);
		return;
	}

	if (State->GatherPhase != EKodGatherPhase::None || State->IsCarryingCargo())
	{
		RefundGatherCargo(*State);
		ClearGatherFields(*State);
	}
	ClearBuildAssignment(*State);
	State->OrderQueue.Reset();
	State->BackswingTicksRemaining = 0;
	State->RetaliateTarget = FKodEntityId();
	State->Order = Kind;
	State->bExplicitAttack = Kind == EKodSimOrderType::Attack;
	State->MoveTarget = Point;
	State->AttackTarget = Kind == EKodSimOrderType::Attack ? Target : FKodEntityId();
	State->FollowTarget = (Kind == EKodSimOrderType::Follow || Kind == EKodSimOrderType::Guard) ? Target : FKodEntityId();
	if (Kind == EKodSimOrderType::Patrol)
	{
		State->PatrolAnchor = State->Position;
		QuantizeResourcePosition(State->PatrolAnchor);
	}
	State->PathOffsetX = OffsetX;
	State->PathOffsetY = OffsetY;
	State->PathCursor = 0;
	if (SharedPathId > 0)
	{
		State->PathId = SharedPathId;
	}
	else if (Kind == EKodSimOrderType::Move || Kind == EKodSimOrderType::AttackMove || Kind == EKodSimOrderType::Patrol)
	{
		State->PathId = FMath::Max(0, PlanMovePath(State->Position, Point, State->TeamId));
	}
	else
	{
		State->PathId = 0;
	}
	if (Kind == EKodSimOrderType::Attack)
	{
		if (const FKodSimEntityState* Victim = States.Find(Target.Value))
		{
			if (Victim->TeamId == State->TeamId || CanTeamSeeWorld(State->TeamId, Victim->Position.X, Victim->Position.Y))
			{
				State->MoveTarget = Victim->Position;
			}
			else
			{
				State->MoveTarget = HiddenTargetPoint(State->TeamId, *Victim, State->Position.Z);
			}
		}
	}
	UE_LOG(LogTemp, Verbose, TEXT("KodOrder %s Unit=%d Target=%d Dest=%.0f,%.0f Path=%d"),
		OrderLabel(Kind),
		Id.Value,
		Target.Value,
		State->MoveTarget.X,
		State->MoveTarget.Y,
		State->PathId);
}

void UKodSimSubsystem::CompleteCurrentOrder(FKodSimEntityState& State, float FixedDt)
{
	State.PathId = 0;
	State.PathCursor = 0;
	State.BackswingTicksRemaining = 0;
	if (State.OrderQueue.Num() > 0)
	{
		TArray<FKodQueuedOrder> Rest = State.OrderQueue;
		const FKodQueuedOrder Step = Rest[0];
		Rest.RemoveAt(0);
		FKodEntityId Target;
		Target.Value = Step.TargetId;
		const FVector Point(static_cast<float>(Step.X), static_cast<float>(Step.Y), State.Position.Z);
		const int32 SelfId = State.Id.Value;
		if (Step.Order == EKodSimOrderType::Gather)
		{
			IssueGather(State.Id, Target);
		}
		else
		{
			IssueOrder(State.Id, Step.Order, Point, Target, false, Step.PathId, Step.OffsetX, Step.OffsetY);
		}
		if (FKodSimEntityState* Live = States.Find(SelfId))
		{
			Live->OrderQueue = Rest;
		}
		UE_LOG(LogTemp, Verbose, TEXT("KodOrder QueuePop Unit=%d Kind=%s"), SelfId, OrderLabel(Step.Order));
		return;
	}
	State.Order = EKodSimOrderType::None;
	State.bExplicitAttack = false;
	State.AttackTarget = FKodEntityId();
	State.FollowTarget = FKodEntityId();
	if (!State.bCanGather && State.WeaponDamage > 0.f && State.bMobile && State.Health > 0.f)
	{
		TryAutoAcquire(State, FixedDt);
		return;
	}
	QuantizePose(State);
}

bool UKodSimSubsystem::TryApplyWeapon(FKodSimEntityState& State, FKodSimEntityState& Target)
{
	if (bCheatInvincible && Target.TeamId == 0)
	{
		return false;
	}
	if (Target.bFlyer)
	{
		const FKodStructureState* AttackerStructure = Structures.Find(State.Id.Value);
		if (!AttackerStructure || !AttackerStructure->bAttacksAir)
		{
			return false;
		}
	}
	if (State.CooldownTicksRemaining > 0 || State.WeaponDamage <= 0.f)
	{
		return false;
	}
	if (State.BackswingTicksRemaining <= 0)
	{
		State.BackswingTicksRemaining = FMath::Max(1, State.WeaponBackswingTicks);
		return false;
	}
	--State.BackswingTicksRemaining;
	if (State.BackswingTicksRemaining > 0)
	{
		return false;
	}

	auto ActorOf = [this](int32 IdValue) -> AActor*
	{
		if (const TWeakObjectPtr<AActor>* Found = Entities.Find(IdValue))
		{
			return Found->Get();
		}
		return nullptr;
	};

	AActor* AttackerActor = ActorOf(State.Id.Value);
	AActor* TargetActor = ActorOf(Target.Id.Value);
	const FString AttackerName = AttackerActor ? AttackerActor->GetName() : FString(TEXT("None"));
	const FString TargetName = TargetActor ? TargetActor->GetName() : FString(TEXT("None"));
	const int32 TargetIdValue = Target.Id.Value;
	const FKodEntityId DeadId = Target.Id;
	const bool bStructureTarget = Structures.Contains(DeadId.Value);
	const int32 AttackerTeam = State.TeamId;
	const int32 AttackerId = State.Id.Value;

	int32 WeaponBonus = 0;
	if (State.bInfantry)
	{
		if (const FKodTeamTech* Tech = TeamTech.Find(AttackerTeam))
		{
			WeaponBonus = Tech->InfantryWeapons;
		}
	}
	int32 Armor = Target.ArmorPoints;
	if (Target.bInfantry)
	{
		if (const FKodTeamTech* Tech = TeamTech.Find(Target.TeamId))
		{
			Armor += Tech->InfantryArmor;
		}
	}
	const float Dealt = FMath::Max(0.5f, State.WeaponDamage + static_cast<float>(WeaponBonus) - static_cast<float>(Armor));
	const int32 DealtInt = FMath::Max(1, FMath::RoundToInt(Dealt));
	if (AttackerTeam == 0)
	{
		DamageDealt0 += DealtInt;
	}
	else
	{
		DamageDealt1 += DealtInt;
	}
	if (Target.TeamId == 0)
	{
		DamageTaken0 += DealtInt;
	}
	else
	{
		DamageTaken1 += DealtInt;
	}
	Target.Health = FMath::Max(0.f, Target.Health - Dealt);
	if (State.DefinitionId == FName(KodWardenPaths::Id_Marine))
	{
		RaiseCue(FName(TEXT("marine_fire")), State.Position, true);
	}
	NotifyOwnedDamage(Target);
	const int32 AttackPermille = FMath::Clamp(State.AbilityAttackPermille, 1, 10000);
	const int32 ScaledTicks = (bCheatNoCooldown && AttackerTeam == 0)
		? 0
		: FMath::Max(1, (FMath::Max(1, State.WeaponCooldownTicks) * 1000) / AttackPermille);
	State.CooldownTicksRemaining = ScaledTicks;
	State.CooldownRemaining = static_cast<float>(ScaledTicks) * KodBuildTicks::SimDt;
	const float HealthLeft = Target.Health;

	const bool bGathering = Target.GatherPhase != EKodGatherPhase::None;
	const bool bIdle = Target.Order == EKodSimOrderType::None;
	const bool bAttackMoving = Target.Order == EKodSimOrderType::AttackMove;
	if (HealthLeft > 0.f
		&& !Target.bCanGather
		&& !bGathering
		&& Target.WeaponDamage > 0.f
		&& Target.TeamId != AttackerTeam
		&& (bIdle || bAttackMoving))
	{
		Target.RetaliateTarget.Value = AttackerId;
	}

	UE_LOG(LogTemp, Log, TEXT("KodSim Fire Attacker=%s Target=%s Tick=%lld"),
		*AttackerName,
		*TargetName,
		static_cast<long long>(SimTickIndex));
	OnUnitFired.Broadcast(AttackerActor, TargetActor, static_cast<int32>(SimTickIndex));
	UE_LOG(LogTemp, Log, TEXT("KodSim Hit Target=%s Id=%d HP=%.0f"), *TargetName, TargetIdValue, HealthLeft);
	OnUnitHit.Broadcast(AttackerActor, TargetActor, HealthLeft, TargetIdValue);
	PushJuiceText(Target.Position, DealtInt, 0);

	if (HealthLeft <= 0.f)
	{
		State.Order = EKodSimOrderType::None;
		State.AttackTarget = FKodEntityId();
		QuantizePose(State);
		FinishEntityDeath(Target, AttackerTeam);
		return true;
	}
	return false;
}

void UKodSimSubsystem::NotifyOwnedDamage(const FKodSimEntityState& Target)
{
	if (Target.TeamId != 0)
	{
		return;
	}
	constexpr int64 WindowTicks = 20 * 16;
	constexpr int64 RadiusSq = 1500ll * 1500ll;
	const int32 AlertX = FMath::RoundToInt(Target.Position.X);
	const int32 AlertY = FMath::RoundToInt(Target.Position.Y);
	AttackAlerts.RemoveAll([this, WindowTicks](const FKodAttackAlert& Alert)
	{
		return SimTickIndex - Alert.Tick >= WindowTicks;
	});
	for (const FKodAttackAlert& Alert : AttackAlerts)
	{
		if (Alert.TeamId != Target.TeamId)
		{
			continue;
		}
		const int64 DX = static_cast<int64>(AlertX) - static_cast<int64>(Alert.X);
		const int64 DY = static_cast<int64>(AlertY) - static_cast<int64>(Alert.Y);
		if (DX * DX + DY * DY <= RadiusSq)
		{
			return;
		}
	}
	FKodAttackAlert Alert;
	Alert.TeamId = Target.TeamId;
	Alert.X = AlertX;
	Alert.Y = AlertY;
	Alert.Tick = SimTickIndex;
	AttackAlerts.Add(Alert);
	AttackAlerts.Sort([](const FKodAttackAlert& A, const FKodAttackAlert& B)
	{
		if (A.Tick != B.Tick)
		{
			return A.Tick < B.Tick;
		}
		if (A.TeamId != B.TeamId)
		{
			return A.TeamId < B.TeamId;
		}
		if (A.X != B.X)
		{
			return A.X < B.X;
		}
		return A.Y < B.Y;
	});
	const bool bStructure = Structures.Contains(Target.Id.Value);
	const FVector At(static_cast<float>(AlertX), static_cast<float>(AlertY), Target.Position.Z);
	RaiseCue(FName(TEXT("alert_ping")), At, false);
	RaiseCue(bStructure ? FName(TEXT("base_under_attack")) : FName(TEXT("units_under_attack")), At, false);
	UE_LOG(LogTemp, Log, TEXT("KodAlert Area X=%d Y=%d Tick=%lld Structure=%d"),
		AlertX,
		AlertY,
		static_cast<long long>(SimTickIndex),
		bStructure ? 1 : 0);
}

void UKodSimSubsystem::StripDeadFromCargo(int32 DeadId)
{
	if (FKodSimEntityState* Dead = States.Find(DeadId))
	{
		Dead->InsideBunker = FKodEntityId();
	}
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		if (FKodStructureState* Structure = Structures.Find(Key))
		{
			Structure->Cargo.Remove(DeadId);
		}
	}
}

void UKodSimSubsystem::FinishEntityDeath(FKodSimEntityState& Target, int32 KillerTeam)
{
	const int32 TargetIdValue = Target.Id.Value;
	const FKodEntityId DeadId = Target.Id;
	const bool bStructureTarget = Structures.Contains(DeadId.Value);
	const FVector DeathAt = Target.Position;
	const FName DefinitionId = Target.DefinitionId;
	AActor* TargetActor = nullptr;
	if (const TWeakObjectPtr<AActor>* Found = Entities.Find(TargetIdValue))
	{
		TargetActor = Found->Get();
	}
	const FString TargetName = TargetActor ? TargetActor->GetName() : FString(TEXT("None"));
	UE_LOG(LogTemp, Log, TEXT("KodSim Kill Target=%s Id=%d"), *TargetName, TargetIdValue);

	int32 Points = 0;
	if (bStructureTarget)
	{
		if (const FKodBuildSpec* Build = BuildSpecs.Find(DefinitionId))
		{
			Points = Build->PowerPoints;
		}
	}
	else if (const FKodTrainSpec* Train = TrainSpecs.Find(DefinitionId))
	{
		Points = Train->PowerPoints;
	}
	AwardPowerPoints(KillerTeam, Points);
	NoteUnitKilled(KillerTeam);
	if (bStructureTarget)
	{
		ScoreTracker.NoteStructureLost(Target.TeamId, DefinitionId);
		if (const FKodStructureState* DeadStructure = Structures.Find(DeadId.Value))
		{
			if (DeadStructure->bComplete)
			{
				NoteStructureRazed(KillerTeam);
				ScoreTracker.NoteStructureRazed(KillerTeam, DefinitionId);
			}
		}
	}
	else
	{
		ScoreTracker.NoteUnitLost(Target.TeamId, DefinitionId);
		ScoreTracker.NoteUnitKilled(KillerTeam, DefinitionId);
	}
	if (DefinitionId == FName(TEXT("IsrPlatform")))
	{
		UE_LOG(LogTemp, Log, TEXT("KodOrbital Down Id=%d Reason=Destroyed"), DeadId.Value);
	}
	bool bOrganicDeath = false;
	if (!bStructureTarget)
	{
		if (const FKodTrainSpec* Train = TrainSpecs.Find(DefinitionId))
		{
			bOrganicDeath = Train->bOrganic;
		}
		else
		{
			bOrganicDeath = Target.bInfantry || Target.bBiological || Target.bCanGather;
		}
	}
	if (!bStructureTarget && !bOrganicDeath)
	{
		RaiseCue(FName(TEXT("unit_death")), DeathAt, true);
	}
	StripDeadFromCargo(DeadId.Value);
	UnregisterEntity(DeadId);
	OnUnitKilled.Broadcast(TargetActor, TargetIdValue);
	if (bStructureTarget && TargetActor && !TargetActor->IsActorBeingDestroyed())
	{
		TargetActor->Destroy();
	}
}

void UKodSimSubsystem::StepHold(FKodSimEntityState& State, float FixedDt)
{
	(void)FixedDt;
	if (State.bCanGather)
	{
		State.RetaliateTarget = FKodEntityId();
		State.AttackTarget = FKodEntityId();
		QuantizePose(State);
		return;
	}
	FKodEntityId Threat = FindNearestEnemyInRange(State);
	if (!Threat.IsValid())
	{
		Threat = FindNearestEnemyStructureInRange(State);
	}
	if (!Threat.IsValid())
	{
		State.AttackTarget = FKodEntityId();
		State.BackswingTicksRemaining = 0;
		QuantizePose(State);
		return;
	}
	FKodSimEntityState* Target = States.Find(Threat.Value);
	if (!Target)
	{
		QuantizePose(State);
		return;
	}
	const float Range = FMath::Max(0.f, State.WeaponRange);
	if (FVector::DistSquared2D(State.Position, Target->Position) > FMath::Square(Range))
	{
		State.AttackTarget = FKodEntityId();
		QuantizePose(State);
		return;
	}
	State.AttackTarget = Threat;
	const FVector ToTarget = Target->Position - State.Position;
	if (ToTarget.SizeSquared2D() > KINDA_SMALL_NUMBER)
	{
		State.YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
	}
	if (TryApplyWeapon(State, *Target))
	{
		return;
	}
	QuantizePose(State);
}

void UKodSimSubsystem::StepPatrol(FKodSimEntityState& State, float FixedDt)
{
	const FVector Dest = State.MoveTarget;
	const int32 SelfId = State.Id.Value;
	if (State.bCanGather)
	{
		State.RetaliateTarget = FKodEntityId();
		State.AttackTarget = FKodEntityId();
		State.bExplicitAttack = false;
		State.Order = EKodSimOrderType::Patrol;
		State.MoveTarget = Dest;
		StepEntityMove(State, FixedDt);
		return;
	}
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
				Live->Order = EKodSimOrderType::Patrol;
				Live->MoveTarget = Dest;
			}
		}
		return;
	}
	State.AttackTarget = FKodEntityId();
	State.Order = EKodSimOrderType::Patrol;
	State.MoveTarget = Dest;
	StepEntityMove(State, FixedDt);
}

void UKodSimSubsystem::StepFollow(FKodSimEntityState& State, float FixedDt, bool bGuard)
{
	FKodSimEntityState* Subject = States.Find(State.FollowTarget.Value);
	if (!Subject || Subject->Health <= 0.f)
	{
		CompleteCurrentOrder(State, FixedDt);
		return;
	}
	const int32 SelfId = State.Id.Value;
	const EKodSimOrderType Kind = bGuard ? EKodSimOrderType::Guard : EKodSimOrderType::Follow;
	const float Stop = static_cast<float>(FMath::Max(40, State.BodyRadiusUU + Subject->BodyRadiusUU + 24));
	State.MoveTarget = Subject->Position;
	if (FVector::DistSquared2D(State.Position, Subject->Position) > FMath::Square(Stop))
	{
		const FVector Kept = State.MoveTarget;
		StepEntityMove(State, FixedDt);
		if (FKodSimEntityState* Live = States.Find(SelfId))
		{
			if (Live->Health > 0.f && Live->Order != Kind)
			{
				Live->Order = Kind;
				Live->MoveTarget = Kept;
			}
		}
	}
	if (!bGuard)
	{
		QuantizePose(State);
		return;
	}
	if (State.bCanGather)
	{
		if (FKodSimEntityState* IdleWorker = States.Find(SelfId))
		{
			IdleWorker->RetaliateTarget = FKodEntityId();
			IdleWorker->AttackTarget = FKodEntityId();
			QuantizePose(*IdleWorker);
		}
		return;
	}
	if (FKodSimEntityState* Live = States.Find(SelfId))
	{
		FKodEntityId Threat = FindNearestEnemyInRange(*Live);
		if (!Threat.IsValid())
		{
			Threat = FindNearestEnemyStructureInRange(*Live);
		}
		if (!Threat.IsValid())
		{
			Live->AttackTarget = FKodEntityId();
			QuantizePose(*Live);
			return;
		}
		FKodSimEntityState* Target = States.Find(Threat.Value);
		if (!Target)
		{
			return;
		}
		const float Range = FMath::Max(0.f, Live->WeaponRange);
		if (FVector::DistSquared2D(Live->Position, Target->Position) > FMath::Square(Range))
		{
			Live->AttackTarget = FKodEntityId();
			QuantizePose(*Live);
			return;
		}
		Live->AttackTarget = Threat;
		const FVector ToTarget = Target->Position - Live->Position;
		if (ToTarget.SizeSquared2D() > KINDA_SMALL_NUMBER)
		{
			Live->YawDegrees = FMath::RadiansToDegrees(FMath::Atan2(ToTarget.Y, ToTarget.X));
		}
		TryApplyWeapon(*Live, *Target);
	}
}
