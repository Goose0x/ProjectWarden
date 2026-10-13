#include "Sim/KodSimSubsystem.h"

#include "Game/KodAIMemorySaveGame.h"
#include "Hash/CityHash.h"
#include "Kismet/GameplayStatics.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	constexpr int32 AiTeamId = 1;
	constexpr int32 AiThinkPeriod = 16;
	constexpr int32 AiWorkerGoal = 16;
	constexpr int32 AiSupplySlack = 4;
	constexpr int32 AiWaveBase = 8;
	constexpr int32 AiWaveCap = 24;
	constexpr int32 AiWaveGapTicks = 16 * 20;
	constexpr int32 AiWaveTimeStep = 16 * 45;

	int64 Dist2DSq(const FVector& A, const FVector& B)
	{
		const int64 DX = static_cast<int64>(FMath::RoundToInt(A.X)) - static_cast<int64>(FMath::RoundToInt(B.X));
		const int64 DY = static_cast<int64>(FMath::RoundToInt(A.Y)) - static_cast<int64>(FMath::RoundToInt(B.Y));
		return DX * DX + DY * DY;
	}
}

void UKodSimSubsystem::SetAiEnabled(bool bEnabled)
{
	const bool bWasEnabled = bAiEnabled;
	bAiEnabled = bEnabled;
	if (!bWasEnabled || bEnabled)
	{
		return;
	}

	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	int32 Cleared = 0;
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->TeamId != AiTeamId || !State->bMobile)
		{
			continue;
		}
		if (State->Order == EKodSimOrderType::None
			&& State->GatherPhase == EKodGatherPhase::None
			&& !State->BuildSite.IsValid())
		{
			continue;
		}
		FKodEntityId Id;
		Id.Value = Key;
		IssueStop(Id);
		++Cleared;
	}
	UE_LOG(LogTemp, Log, TEXT("KodAI Orders Cleared Count=%d"), Cleared);
}

void UKodSimSubsystem::SetAiDifficulty(int32 Difficulty)
{
	AiDifficulty = FMath::Max(1, Difficulty);
}

void UKodSimSubsystem::SetAiSeeded(bool bSeeded)
{
	bAiSeeded = bSeeded;
}

void UKodSimSubsystem::SetEnemyStart(FVector WorldLocation)
{
	bHasEnemyStart = true;
	EnemyStartX = FMath::RoundToInt(WorldLocation.X);
	EnemyStartY = FMath::RoundToInt(WorldLocation.Y);
}

bool UKodSimSubsystem::TryGetEnemyStart(FVector& OutPoint) const
{
	if (bHasEnemyStart)
	{
		OutPoint = FVector(static_cast<float>(EnemyStartX), static_cast<float>(EnemyStartY), 0.f);
		return true;
	}
	if (!bHasPlayable)
	{
		return false;
	}
	FVector Base = FVector::ZeroVector;
	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	for (int32 Key : StructureKeys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->TeamId != AiTeamId)
		{
			continue;
		}
		Base = Structure->Position;
		if (Structure->DefinitionId == FName(KodWardenPaths::Id_CommandCenter))
		{
			break;
		}
	}
	const int32 Inset = 3;
	const int32 Left = PlayMinX + Inset;
	const int32 Right = FMath::Max(Left, PlayMaxX - Inset);
	const int32 Bottom = PlayMinY + Inset;
	const int32 Top = FMath::Max(Bottom, PlayMaxY - Inset);
	const int32 Corners[4][2] = {
		{Left, Bottom},
		{Left, Top},
		{Right, Bottom},
		{Right, Top}
	};
	int32 BestX = Left;
	int32 BestY = Bottom;
	int64 Best = -1;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float X = KodBuildGrid::CellCenter(Corners[Index][0]);
		const float Y = KodBuildGrid::CellCenter(Corners[Index][1]);
		const int64 DX = static_cast<int64>(FMath::RoundToInt(X - Base.X));
		const int64 DY = static_cast<int64>(FMath::RoundToInt(Y - Base.Y));
		const int64 Dist = DX * DX + DY * DY;
		const bool bTie = Dist == Best && (Corners[Index][0] < BestX || (Corners[Index][0] == BestX && Corners[Index][1] < BestY));
		if (Dist > Best || bTie)
		{
			Best = Dist;
			BestX = Corners[Index][0];
			BestY = Corners[Index][1];
		}
	}
	OutPoint = FVector(KodBuildGrid::CellCenter(BestX), KodBuildGrid::CellCenter(BestY), 0.f);
	return true;
}

void UKodSimSubsystem::EvaluateMatch()
{
	if (bTestIsolation || MatchOutcome != EKodMatchOutcome::None)
	{
		return;
	}

	int32 Team0 = 0;
	int32 Team1 = 0;
	TArray<int32> Keys;
	Structures.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		// A lifted structure still counts toward victory.
		if (!Structure)
		{
			continue;
		}
		if (Structure->TeamId == 0)
		{
			++Team0;
		}
		else if (Structure->TeamId == AiTeamId)
		{
			++Team1;
		}
	}
	if (Team0 > 0)
	{
		bTeam0HadStructure = true;
	}
	if (Team1 > 0)
	{
		bTeam1HadStructure = true;
	}
	if (bTeam0HadStructure && Team0 == 0)
	{
		MatchOutcome = EKodMatchOutcome::Defeat;
		UE_LOG(LogTemp, Log, TEXT("KodAI Match Defeat"));
		RaiseCue(FName(TEXT("defeat")), FVector::ZeroVector, false);
		RememberAiMatch();
	}
	else if (bTeam1HadStructure && Team1 == 0)
	{
		MatchOutcome = EKodMatchOutcome::Victory;
		UE_LOG(LogTemp, Log, TEXT("KodAI Match Victory"));
		RaiseCue(FName(TEXT("victory")), FVector::ZeroVector, false);
		RememberAiMatch();
	}
}

void UKodSimSubsystem::ChooseAiOpener()
{
	if (bAiOpenerChosen)
	{
		return;
	}
	const FName Openers[3] = {
		FName(TEXT("MacroUpgrades")),
		FName(TEXT("SafeExpand")),
		FName(TEXT("TimingAttack"))
	};
	int32 Weights[3] = {1, 1, 1};
	int32 WinSum = 0;
	int32 LossSum = 0;
	if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(TEXT("KodAIMemory"), 0))
	{
		if (const UKodAIMemorySaveGame* Memory = Cast<UKodAIMemorySaveGame>(Loaded))
		{
			for (const FKodOpenerRecord& Record : Memory->Records)
			{
				WinSum += FMath::Max(0, Record.Wins);
				LossSum += FMath::Max(0, Record.Losses);
				for (int32 Index = 0; Index < 3; ++Index)
				{
					if (Record.Opener == Openers[Index])
					{
						Weights[Index] = 1 + FMath::Max(0, Record.Wins) * 2;
						if (Record.Losses > Record.Wins)
						{
							Weights[Index] = 1;
						}
					}
				}
			}
		}
	}
	int32 Total = 0;
	for (int32 Weight : Weights)
	{
		Total += FMath::Max(1, Weight);
	}
	uint8 SeedBytes[12];
	const int32 Diff = FMath::Max(1, AiDifficulty);
	FMemory::Memcpy(SeedBytes, &Diff, sizeof(Diff));
	FMemory::Memcpy(SeedBytes + 4, &WinSum, sizeof(WinSum));
	FMemory::Memcpy(SeedBytes + 8, &LossSum, sizeof(LossSum));
	const uint64 Seed = CityHash64(reinterpret_cast<const char*>(SeedBytes), sizeof(SeedBytes));
	int32 Cursor = static_cast<int32>(Seed % static_cast<uint64>(FMath::Max(1, Total)));
	int32 Picked = 0;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const int32 Weight = FMath::Max(1, Weights[Index]);
		if (Cursor < Weight)
		{
			Picked = Index;
			break;
		}
		Cursor -= Weight;
	}
	AiOpener = Picked;
	bAiOpenerChosen = true;
	UE_LOG(LogTemp, Log, TEXT("KodAI Opener=%s"), *Openers[Picked].ToString());
}

void UKodSimSubsystem::RememberAiMatch()
{
	if (bAiMemoryWritten || !bAiOpenerChosen)
	{
		return;
	}
	const FName Openers[3] = {
		FName(TEXT("MacroUpgrades")),
		FName(TEXT("SafeExpand")),
		FName(TEXT("TimingAttack"))
	};
	const int32 Index = FMath::Clamp(AiOpener, 0, 2);
	UKodAIMemorySaveGame* Memory = Cast<UKodAIMemorySaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("KodAIMemory"), 0));
	if (!Memory)
	{
		Memory = NewObject<UKodAIMemorySaveGame>();
	}
	FKodOpenerRecord* Record = nullptr;
	for (FKodOpenerRecord& Existing : Memory->Records)
	{
		if (Existing.Opener == Openers[Index])
		{
			Record = &Existing;
			break;
		}
	}
	if (!Record)
	{
		FKodOpenerRecord Added;
		Added.Opener = Openers[Index];
		Memory->Records.Add(Added);
		Memory->Records.Sort([](const FKodOpenerRecord& A, const FKodOpenerRecord& B)
		{
			return A.Opener.ToString() < B.Opener.ToString();
		});
		for (FKodOpenerRecord& Existing : Memory->Records)
		{
			if (Existing.Opener == Openers[Index])
			{
				Record = &Existing;
				break;
			}
		}
	}
	if (!Record)
	{
		return;
	}
	if (MatchOutcome == EKodMatchOutcome::Defeat)
	{
		++Record->Wins;
	}
	else if (MatchOutcome == EKodMatchOutcome::Victory)
	{
		++Record->Losses;
	}
	bAiMemoryWritten = true;
	UGameplayStatics::SaveGameToSlot(Memory, TEXT("KodAIMemory"), 0);
	UE_LOG(LogTemp, Log, TEXT("KodAI Memory Opener=%s Wins=%d Losses=%d"),
		*Record->Opener.ToString(),
		Record->Wins,
		Record->Losses);
}

void UKodSimSubsystem::SetPlayerAutoplay(bool bEnabled)
{
	if (bEnabled && !AreCheatsAllowed())
	{
		bPlayerAutoplay = false;
		UE_LOG(LogTemp, Log, TEXT("KodAutoplay refused (campaign and offline only)"));
		return;
	}
	bPlayerAutoplay = bEnabled;
	UE_LOG(LogTemp, Log, TEXT("KodAutoplay %d"), bPlayerAutoplay ? 1 : 0);
}

void UKodSimSubsystem::StepPlayerAutoplay()
{
	if (!bPlayerAutoplay || !AreCheatsAllowed() || MatchOutcome != EKodMatchOutcome::None)
	{
		return;
	}
	if (AiThinkPeriod <= 0 || (SimTickIndex % AiThinkPeriod) != 0)
	{
		return;
	}

	const FName DozerId(KodWardenPaths::Id_Dozer);
	const FName CcTypeId(KodWardenPaths::Id_CommandCenter);
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		FKodSimEntityState* State = States.Find(Key);
		if (!State
			|| State->TeamId != 0
			|| !State->bCanGather
			|| State->Health <= 0.f
			|| State->Order != EKodSimOrderType::None
			|| State->GatherPhase != EKodGatherPhase::None
			|| State->BuildSite.IsValid())
		{
			continue;
		}
		FKodEntityId Node = FindNearestResourceNodeOfType(State->Position, EKodResourceType::Jadeite, true);
		if (!Node.IsValid())
		{
			Node = FindNearestResourceNodeOfType(State->Position, EKodResourceType::Luminene, false);
		}
		if (Node.IsValid())
		{
			DeferGather(State->Id, Node);
		}
	}

	int32 Workers = 0;
	int32 Queued = 0;
	FKodEntityId CcId;
	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	for (int32 Key : StructureKeys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->TeamId != 0)
		{
			continue;
		}
		if (Structure->DefinitionId == CcTypeId && Structure->bComplete && !CcId.IsValid())
		{
			CcId = Structure->Id;
		}
		for (const FKodProductionSlot& Slot : Structure->Slots)
		{
			if (Slot.UnitId == DozerId)
			{
				++Queued;
			}
		}
	}
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (State && State->TeamId == 0 && State->Health > 0.f && State->bCanGather)
		{
			++Workers;
		}
	}
	if (CcId.IsValid() && Workers + Queued < 8)
	{
		DeferTrain(CcId, DozerId);
	}
	const FName MarineId(KodWardenPaths::Id_Marine);
	const FName BarracksId(KodWardenPaths::Id_Barracks);
	int32 Combat = 0;
	int32 QueuedMarines = 0;
	FKodEntityId Barracks;
	for (int32 Key : StructureKeys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->TeamId != 0)
		{
			continue;
		}
		if (Structure->DefinitionId == BarracksId && Structure->bComplete && !Barracks.IsValid())
		{
			Barracks = Structure->Id;
		}
		for (const FKodProductionSlot& Slot : Structure->Slots)
		{
			if (Slot.UnitId == MarineId)
			{
				++QueuedMarines;
			}
		}
	}
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (State && State->TeamId == 0 && State->Health > 0.f && !State->bCanGather && State->WeaponDamage > 0.f)
		{
			++Combat;
		}
	}
	if (Barracks.IsValid() && Combat + QueuedMarines < 8)
	{
		DeferTrain(Barracks, MarineId);
	}

	if (SimTickIndex < PlayerNextWaveTick)
	{
		return;
	}
	FVector AttackPoint = FVector::ZeroVector;
	bool bAttack = false;
	int64 Best = 0;
	FKodEntityId Target;
	for (int32 Key : StructureKeys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->TeamId == 0)
		{
			continue;
		}
		const int64 DX = static_cast<int64>(FMath::RoundToInt(Structure->Position.X));
		const int64 DY = static_cast<int64>(FMath::RoundToInt(Structure->Position.Y));
		const int64 Dist = DX * DX + DY * DY;
		if (!bAttack || Dist < Best)
		{
			bAttack = true;
			Best = Dist;
			AttackPoint = Structure->Position;
			Target = Structure->Id;
		}
	}
	if (!bAttack && bFaultlineTraining)
	{
		bAttack = true;
		AttackPoint = FVector(static_cast<float>(FaultlineOutpostX), static_cast<float>(FaultlineOutpostY), static_cast<float>(FaultlineOutpostZ));
	}
	if (!bAttack)
	{
		return;
	}
	if (!bAutoplaySquadSent && Combat < 4 && SimTickIndex >= 8 * 16)
	{
		FVector Origin = AttackPoint;
		if (CcId.IsValid())
		{
			if (const FKodStructureState* CommandCenter = Structures.Find(CcId.Value))
			{
				Origin = CommandCenter->Position;
			}
		}
		else
		{
			for (int32 Key : Keys)
			{
				const FKodSimEntityState* Worker = States.Find(Key);
				if (Worker && Worker->TeamId == 0 && Worker->bCanGather && Worker->Health > 0.f)
				{
					Origin = Worker->Position;
					break;
				}
			}
		}
		EnqueueAutoplaySquad(Origin, AttackPoint);
	}
	int32 Sent = 0;
	for (int32 Key : Keys)
	{
		FKodSimEntityState* State = States.Find(Key);
		if (!State
			|| State->TeamId != 0
			|| State->Health <= 0.f
			|| !State->bMobile
			|| State->bCanGather)
		{
			continue;
		}
		if (State->WeaponDamage <= 0.f)
		{
			State->WeaponDamage = 6.f;
			if (State->WeaponRange <= 0.f)
			{
				State->WeaponRange = 500.f;
			}
		}
		FKodEntityId UnitId;
		UnitId.Value = Key;
		const FVector* OrderMark = AutoplayOrderMarks.Find(Key);
		const bool bStuck = OrderMark && FVector::DistSquared2D(State->Position, *OrderMark) < FMath::Square(64.f);
		const bool bOnTarget = Target.IsValid()
			&& State->Order == EKodSimOrderType::Attack
			&& State->AttackTarget == Target;
		const bool bAlreadyMoving = State->Order == EKodSimOrderType::AttackMove
			&& FVector::DistSquared2D(State->MoveTarget, AttackPoint) <= FMath::Square(128.f);
		if ((bOnTarget || bAlreadyMoving) && !bStuck)
		{
			AutoplayOrderMarks.Add(Key, State->Position);
			continue;
		}
		if (Target.IsValid() && States.Contains(Target.Value))
		{
			DeferAttack(UnitId, Target);
		}
		else
		{
			DeferAttackMove(UnitId, AttackPoint);
		}
		AutoplayOrderMarks.Add(Key, State->Position);
		++Sent;
		if (Sent >= 8)
		{
			break;
		}
	}
	if (Sent > 0)
	{
		PlayerNextWaveTick = SimTickIndex + static_cast<int64>(AiWaveGapTicks);
		UE_LOG(LogTemp, Log, TEXT("KodAutoplay Attack Sent=%d Tick=%lld"), Sent, static_cast<long long>(SimTickIndex));
	}
}

void UKodSimSubsystem::EnqueueAutoplaySquad(const FVector& Origin, const FVector& AttackPoint)
{
	if (bAutoplaySquadSent)
	{
		return;
	}
	bAutoplaySquadSent = true;
	for (int32 Slot = 0; Slot < 6; ++Slot)
	{
		FKodPendingUnitSpawn Spawn;
		Spawn.UnitId = FName(KodWardenPaths::Id_Marine);
		Spawn.TeamId = 0;
		Spawn.Location = Origin + FVector(180.f * static_cast<float>(Slot - 2), 220.f, Origin.Z);
		Spawn.bAttackMove = true;
		Spawn.RallyPoint = AttackPoint;
		PendingSpawns.Add(Spawn);
	}
	UE_LOG(LogTemp, Log, TEXT("KodAutoplay Squad Marines=6 Target=%.0f,%.0f"), AttackPoint.X, AttackPoint.Y);
}

void UKodSimSubsystem::StepAI()
{
	if (bTestIsolation || MatchOutcome != EKodMatchOutcome::None)
	{
		return;
	}
	if (bPlayerAutoplay)
	{
		StepPlayerAutoplay();
	}
	if (!bAiEnabled || !bAiSeeded)
	{
		return;
	}
	if (AiThinkPeriod <= 0 || (SimTickIndex % AiThinkPeriod) != 0)
	{
		return;
	}

	ChooseAiOpener();

	const FName DozerId(KodWardenPaths::Id_Dozer);
	const FName MarineUnitId(KodWardenPaths::Id_Marine);
	const FName DepotId(KodWardenPaths::Id_SupplyDepot);
	const FName BarracksId(KodWardenPaths::Id_Barracks);
	const FName CcTypeId(KodWardenPaths::Id_CommandCenter);
	const FName EngBayId(KodWardenPaths::Id_EngineeringBay);
	const FName MedicId(KodWardenPaths::Id_Medic);

	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();

	for (int32 Key : Keys)
	{
		FKodSimEntityState* State = States.Find(Key);
		if (!State
			|| State->TeamId != AiTeamId
			|| !State->bCanGather
			|| State->Health <= 0.f
			|| State->Order != EKodSimOrderType::None
			|| State->GatherPhase != EKodGatherPhase::None
			|| State->BuildSite.IsValid())
		{
			continue;
		}
		FKodEntityId Node = FindNearestResourceNodeOfType(State->Position, EKodResourceType::Jadeite, true);
		if (!Node.IsValid())
		{
			Node = FindNearestResourceNodeOfType(State->Position, EKodResourceType::Luminene, false);
		}
		if (Node.IsValid())
		{
			DeferGather(State->Id, Node);
		}
	}

	int32 WorkerCount = 0;
	int32 QueuedWorkers = 0;
	int32 CcCount = 0;
	int32 MarineCount = 0;
	bool bDepotInProgress = false;
	bool bDepotComplete = false;
	bool bHasBarracks = false;
	bool bCcInProgress = false;
	bool bEngBayComplete = false;
	bool bEngBayStarted = false;
	FKodEntityId CcId;
	FKodEntityId BarracksIdValue;
	FKodEntityId EngBayEntity;
	FVector Base = FVector::ZeroVector;
	bool bHaveBase = false;

	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	for (int32 Key : StructureKeys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->TeamId != AiTeamId)
		{
			continue;
		}
		if (Structure->DefinitionId == CcTypeId)
		{
			++CcCount;
			if (!Structure->bComplete)
			{
				bCcInProgress = true;
			}
			if (!bHaveBase)
			{
				bHaveBase = true;
				Base = Structure->Position;
				CcId = Structure->Id;
			}
		}
		if (Structure->DefinitionId == EngBayId)
		{
			bEngBayStarted = true;
			if (Structure->bComplete)
			{
				bEngBayComplete = true;
				if (!EngBayEntity.IsValid())
				{
					EngBayEntity = Structure->Id;
				}
			}
		}
		if (Structure->DefinitionId == DepotId)
		{
			if (Structure->bComplete)
			{
				bDepotComplete = true;
			}
			else
			{
				bDepotInProgress = true;
			}
		}
		if (Structure->DefinitionId == BarracksId)
		{
			bHasBarracks = true;
			if (Structure->bComplete && !BarracksIdValue.IsValid())
			{
				BarracksIdValue = Structure->Id;
			}
		}
		for (const FKodProductionSlot& Slot : Structure->Slots)
		{
			if (Slot.UnitId == DozerId)
			{
				++QueuedWorkers;
			}
		}
	}

	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->Health <= 0.f)
		{
			continue;
		}
		if (State->TeamId == AiTeamId && State->bCanGather)
		{
			++WorkerCount;
		}
		if (State->TeamId == AiTeamId && State->bInfantry && State->WeaponDamage > 0.f && !State->bHealer)
		{
			++MarineCount;
		}
		if (State->TeamId == 0 && CanTeamSeeWorld(AiTeamId, State->Position.X, State->Position.Y))
		{
			bScoutSawEnemy = true;
		}
	}
	for (int32 Key : StructureKeys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (Structure && Structure->TeamId == 0 && CanTeamSeeWorld(AiTeamId, Structure->Position.X, Structure->Position.Y))
		{
			bScoutSawEnemy = true;
		}
	}

	int32 Used = 0;
	int32 Cap = 0;
	TryGetSupply(AiTeamId, Used, Cap);
	const bool bNeedDepot = Cap > 0 && Used + AiSupplySlack >= Cap;

	auto LowestFreeWorker = [this, &Keys]() -> FKodEntityId
	{
		FKodEntityId Found;
		for (int32 Key : Keys)
		{
			const FKodSimEntityState* State = States.Find(Key);
			if (!State || State->TeamId != AiTeamId || !State->bCanGather || State->Health <= 0.f || State->BuildSite.IsValid())
			{
				continue;
			}
			Found.Value = Key;
			break;
		}
		return Found;
	};

	auto TryBuildNearBase = [this, &bHaveBase, &Base, &LowestFreeWorker](FName TypeId) -> bool
	{
		if (!bHaveBase)
		{
			return false;
		}
		const FKodBuildSpec* Spec = BuildSpecs.Find(TypeId);
		if (!Spec)
		{
			return false;
		}
		FKodResourceCost Price;
		Price.Jadeite = FMath::Max(0, Spec->Cost.Credits);
		Price.Luminene = FMath::Max(0, Spec->Cost.Luminene);
		if (!CanAfford(AiTeamId, Price))
		{
			return false;
		}
		const FKodEntityId WorkerId = LowestFreeWorker();
		if (!WorkerId.IsValid())
		{
			return false;
		}
		const int32 BaseCellX = KodBuildGrid::CellOf(Base.X);
		const int32 BaseCellY = KodBuildGrid::CellOf(Base.Y);
		for (int32 Radius = 3; Radius <= 40; ++Radius)
		{
			for (int32 X = BaseCellX - Radius; X <= BaseCellX + Radius; ++X)
			{
				for (int32 Y = BaseCellY - Radius; Y <= BaseCellY + Radius; ++Y)
				{
					const int32 Cheb = FMath::Max(FMath::Abs(X - BaseCellX), FMath::Abs(Y - BaseCellY));
					if (Cheb != Radius)
					{
						continue;
					}
					const FVector Cursor(KodBuildGrid::CellCenter(X), KodBuildGrid::CellCenter(Y), Base.Z);
					const FKodPlacementResult Placement = QueryPlacement(AiTeamId, TypeId, Cursor);
					if (!Placement.bValid)
					{
						continue;
					}
					return TryPlaceStructure(AiTeamId, TypeId, Cursor, WorkerId);
				}
			}
		}
		return false;
	};

	const int32 Diff = FMath::Clamp(AiDifficulty, 1, 4);
	const int32 MarginSeconds[5] = {0, 90, 60, 45, 30};
	int32 Margin = MarginSeconds[Diff];
	if (AiOpener == 1)
	{
		Margin += 20;
	}
	else if (AiOpener == 2)
	{
		Margin = FMath::Max(20, Margin - 15);
	}
	if (bScoutSawEnemy && AiOpener == 1)
	{
		Margin += 15;
	}
	int32 ExpandAt = ExposedTicks - (71 * 16) - Margin * 16;
	if (ExpandAt < 60 * 16)
	{
		ExpandAt = 60 * 16;
	}
	const bool bWantSecondCc = CcCount < 2 && !bCcInProgress && SimTickIndex >= ExpandAt;

	const bool bDepotBeforeBarracks = !bHasBarracks && !bDepotComplete;
	if (bWantSecondCc)
	{
		if (TryBuildNearBase(CcTypeId))
		{
			UE_LOG(LogTemp, Log, TEXT("KodAI Expand Tick=%lld"), static_cast<long long>(SimTickIndex));
		}
	}
	else if (bDepotBeforeBarracks)
	{
		if (!bDepotInProgress)
		{
			TryBuildNearBase(DepotId);
		}
	}
	else if (bNeedDepot && !bDepotInProgress)
	{
		TryBuildNearBase(DepotId);
	}
	else if (!bHasBarracks)
	{
		TryBuildNearBase(BarracksId);
	}
	else if (Diff >= 2 && bHasBarracks && !bEngBayStarted)
	{
		TryBuildNearBase(EngBayId);
	}

	int32 ReserveCredits = 0;
	if (!bHasBarracks)
	{
		if (const FKodBuildSpec* BarracksSpec = BuildSpecs.Find(BarracksId))
		{
			ReserveCredits += FMath::Max(0, BarracksSpec->Cost.Credits);
		}
	}
	if (bDepotBeforeBarracks || (bNeedDepot && !bDepotInProgress))
	{
		if (const FKodBuildSpec* DepotSpec = BuildSpecs.Find(DepotId))
		{
			ReserveCredits += FMath::Max(0, DepotSpec->Cost.Credits);
		}
	}
	if (bWantSecondCc)
	{
		if (const FKodBuildSpec* CcSpec = BuildSpecs.Find(CcTypeId))
		{
			ReserveCredits += FMath::Max(0, CcSpec->Cost.Credits);
		}
	}

	auto EnqueueIfOpen = [this](FKodEntityId StructureId, FName UnitId, int32 Reserve)
	{
		const FKodStructureState* Structure = Structures.Find(StructureId.Value);
		if (!Structure || !Structure->bComplete || Structure->Slots.Num() >= KodEconomyDefaults::ProductionSlotCap)
		{
			return;
		}
		FKodTrainSpec Spec;
		if (!TryGetTrainSpec(UnitId, Spec))
		{
			return;
		}
		const int32 PriceCredits = FMath::Max(0, Spec.Cost.Credits);
		FKodResourceCost Bank;
		if (!TryGetBank(Structure->TeamId, Bank) || Bank.Jadeite < PriceCredits + Reserve)
		{
			return;
		}
		FKodResourceCost Price;
		Price.Jadeite = PriceCredits;
		Price.Luminene = FMath::Max(0, Spec.Cost.Luminene);
		if (!CanAfford(Structure->TeamId, Price))
		{
			return;
		}
		EnqueueTrain(StructureId, UnitId);
	};

	if (CcId.IsValid() && WorkerCount + QueuedWorkers < AiWorkerGoal)
	{
		EnqueueIfOpen(CcId, DozerId, ReserveCredits);
	}
	if (BarracksIdValue.IsValid())
	{
		const bool bWantMedic = Diff >= 2 && bEngBayComplete && MarineCount >= 4 && ((SimTickIndex / AiThinkPeriod) % 4) == 0;
		EnqueueIfOpen(BarracksIdValue, bWantMedic ? MedicId : MarineUnitId, bNeedDepot && !bDepotInProgress ? ReserveCredits : 0);
	}
	if (EngBayEntity.IsValid() && Diff >= 2)
	{
		const FKodTeamTech* Tech = TeamTech.Find(AiTeamId);
		if (!Tech || Tech->InfantryWeapons <= 0)
		{
			EnqueueResearch(EngBayEntity, FName(KodWardenPaths::Id_InfantryWeapons));
		}
	}

	if (!bAiScoutSent)
	{
		FVector ScoutDest;
		if (TryGetEnemyStart(ScoutDest))
		{
			FKodEntityId ScoutId;
			if (WorkerCount > 1)
			{
				for (int32 Key : Keys)
				{
					const FKodSimEntityState* State = States.Find(Key);
					if (!State || State->TeamId != AiTeamId || !State->bCanGather || State->Health <= 0.f || State->BuildSite.IsValid())
					{
						continue;
					}
					ScoutId.Value = Key;
					break;
				}
			}
			if (!ScoutId.IsValid())
			{
				for (int32 Key : Keys)
				{
					const FKodSimEntityState* State = States.Find(Key);
					if (!State || State->TeamId != AiTeamId || State->Health <= 0.f || State->bCanGather || State->WeaponDamage <= 0.f)
					{
						continue;
					}
					ScoutId.Value = Key;
					break;
				}
			}
			if (!ScoutId.IsValid())
			{
				for (int32 Key : Keys)
				{
					const FKodSimEntityState* State = States.Find(Key);
					if (!State || State->TeamId != AiTeamId || !State->bCanGather || State->Health <= 0.f || State->BuildSite.IsValid())
					{
						continue;
					}
					ScoutId.Value = Key;
					break;
				}
			}
			if (ScoutId.IsValid())
			{
				IssueAttackMove(ScoutId, ScoutDest);
				bAiScoutSent = true;
				UE_LOG(LogTemp, Log, TEXT("KodAI Scout Unit=%d Dest=%.0f,%.0f"), ScoutId.Value, ScoutDest.X, ScoutDest.Y);
			}
		}
	}

	if (SimTickIndex < AiNextWaveTick)
	{
		return;
	}

	TArray<int32> IdleMarines;
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State
			|| State->TeamId != AiTeamId
			|| State->Health <= 0.f
			|| State->bCanGather
			|| State->WeaponDamage <= 0.f
			|| State->Order != EKodSimOrderType::None)
		{
			continue;
		}
		IdleMarines.Add(Key);
	}

	const int32 Difficulty = FMath::Max(1, AiDifficulty);
	const int32 FromTime = AiWaveTimeStep > 0 ? static_cast<int32>(SimTickIndex / AiWaveTimeStep) * Difficulty : 0;
	int32 WaveSize = FMath::Clamp(AiWaveBase + FromTime + AiWavesLaunched * Difficulty, AiWaveBase, AiWaveCap);
	if (bScoutSawEnemy && AiOpener == 2)
	{
		WaveSize = FMath::Max(4, WaveSize / 2);
	}
	const int32 NeedPct[5] = {0, 70, 100, 120, 140};
	const int32 OwnArmy = ArmyValueFor(AiTeamId);
	int32 SeenEnemy = 0;
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->TeamId != 0 || State->Health <= 0.f || !State->bMobile)
		{
			continue;
		}
		if (!CanTeamSeeWorld(AiTeamId, State->Position.X, State->Position.Y))
		{
			continue;
		}
		FKodTrainSpec Spec;
		if (TryGetTrainSpec(State->DefinitionId, Spec))
		{
			SeenEnemy += FMath::Max(0, Spec.Cost.Credits);
		}
	}
	const int32 UsedDiff = FMath::Clamp(Difficulty, 1, 4);
	if (SeenEnemy > 0 && OwnArmy * 100 < SeenEnemy * NeedPct[UsedDiff])
	{
		for (int32 MarineKey : IdleMarines)
		{
			FKodEntityId MarineId;
			MarineId.Value = MarineKey;
			IssueMove(MarineId, bHaveBase ? Base : FVector::ZeroVector);
		}
		if (!bAiRetreating)
		{
			bAiRetreating = true;
			UE_LOG(LogTemp, Log, TEXT("KodAI Retreat Own=%d Enemy=%d"), OwnArmy, SeenEnemy);
		}
		return;
	}
	bAiRetreating = false;
	if (IdleMarines.Num() < WaveSize)
	{
		return;
	}

	FKodEntityId Target;
	FVector AttackPoint = FVector::ZeroVector;
	bool bAttackPoint = false;
	int64 BestDist = 0;
	const FKodSimEntityState* Lead = States.Find(IdleMarines[0]);
	const FVector From = Lead ? Lead->Position : Base;
	for (int32 Key : StructureKeys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->TeamId != 0 || !States.Contains(Structure->Id.Value))
		{
			continue;
		}
		if (!CanTeamSeeWorld(AiTeamId, Structure->Position.X, Structure->Position.Y))
		{
			continue;
		}
		const int64 Dist = Dist2DSq(From, Structure->Position);
		if (!bAttackPoint || Dist < BestDist)
		{
			bAttackPoint = true;
			BestDist = Dist;
			Target = Structure->Id;
			AttackPoint = Structure->Position;
		}
	}
	if (!Target.IsValid())
	{
		bAttackPoint = false;
		BestDist = 0;
		if (const TArray<FKodSeenStructure>* Seen = SeenStructures.Find(AiTeamId))
		{
			for (const FKodSeenStructure& Entry : *Seen)
			{
				if (Entry.TeamId != 0)
				{
					continue;
				}
				const FVector Point(static_cast<float>(Entry.PosX), static_cast<float>(Entry.PosY), From.Z);
				const int64 Dist = Dist2DSq(From, Point);
				if (!bAttackPoint || Dist < BestDist)
				{
					bAttackPoint = true;
					BestDist = Dist;
					AttackPoint = Point;
					Target.Value = Entry.Id;
				}
			}
		}
	}
	if (!bAttackPoint && !TryGetEnemyStart(AttackPoint))
	{
		return;
	}
	AttackPoint.Z = From.Z;
	const bool bVisibleTarget = Target.IsValid()
		&& States.Contains(Target.Value)
		&& CanTeamSeeWorld(AiTeamId, AttackPoint.X, AttackPoint.Y);

	for (int32 Index = 0; Index < WaveSize && Index < IdleMarines.Num(); ++Index)
	{
		FKodEntityId MarineId;
		MarineId.Value = IdleMarines[Index];
		if (bVisibleTarget)
		{
			IssueAttack(MarineId, Target);
		}
		else
		{
			IssueAttackMove(MarineId, AttackPoint);
		}
	}
	++AiWavesLaunched;
	AiNextWaveTick = SimTickIndex + static_cast<int64>(AiWaveGapTicks);
	UE_LOG(LogTemp, Log, TEXT("KodAI Wave Size=%d Target=%d Dest=%.0f,%.0f"),
		WaveSize,
		Target.Value,
		AttackPoint.X,
		AttackPoint.Y);
}
