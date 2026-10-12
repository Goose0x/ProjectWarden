#include "Sim/KodSimSubsystem.h"

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

void UKodSimSubsystem::EvaluateMatch()
{
	if (MatchOutcome != EKodMatchOutcome::None)
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
	}
	else if (bTeam1HadStructure && Team1 == 0)
	{
		MatchOutcome = EKodMatchOutcome::Victory;
		UE_LOG(LogTemp, Log, TEXT("KodAI Match Victory"));
	}
}

void UKodSimSubsystem::StepAI()
{
	if (!bAiEnabled || !bAiSeeded || MatchOutcome != EKodMatchOutcome::None)
	{
		return;
	}
	if (AiThinkPeriod <= 0 || (SimTickIndex % AiThinkPeriod) != 0)
	{
		return;
	}

	const FName DozerId(KodWardenPaths::Id_Dozer);
	const FName RangerId(KodWardenPaths::Id_Ranger);
	const FName DepotId(KodWardenPaths::Id_SupplyDepot);
	const FName BarracksId(KodWardenPaths::Id_Barracks);

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
			IssueGather(State->Id, Node);
		}
	}

	int32 WorkerCount = 0;
	int32 QueuedWorkers = 0;
	bool bDepotInProgress = false;
	bool bHasBarracks = false;
	FKodEntityId CcId;
	FKodEntityId BarracksIdValue;
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
		if (!bHaveBase && Structure->DefinitionId == FName(KodWardenPaths::Id_CommandCenter))
		{
			bHaveBase = true;
			Base = Structure->Position;
			CcId = Structure->Id;
		}
		if (Structure->DefinitionId == DepotId && !Structure->bComplete)
		{
			bDepotInProgress = true;
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
		if (State && State->TeamId == AiTeamId && State->bCanGather && State->Health > 0.f)
		{
			++WorkerCount;
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

	if (bNeedDepot && !bDepotInProgress)
	{
		TryBuildNearBase(DepotId);
	}
	else if (!bHasBarracks)
	{
		TryBuildNearBase(BarracksId);
	}

	int32 ReserveCredits = 0;
	if (!bHasBarracks)
	{
		if (const FKodBuildSpec* BarracksSpec = BuildSpecs.Find(BarracksId))
		{
			ReserveCredits += FMath::Max(0, BarracksSpec->Cost.Credits);
		}
	}
	if (bNeedDepot && !bDepotInProgress)
	{
		if (const FKodBuildSpec* DepotSpec = BuildSpecs.Find(DepotId))
		{
			ReserveCredits += FMath::Max(0, DepotSpec->Cost.Credits);
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
		EnqueueIfOpen(BarracksIdValue, RangerId, bNeedDepot && !bDepotInProgress ? ReserveCredits : 0);
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
	const int32 WaveSize = FMath::Clamp(AiWaveBase + FromTime + AiWavesLaunched * Difficulty, AiWaveBase, AiWaveCap);
	if (IdleMarines.Num() < WaveSize)
	{
		return;
	}

	FKodEntityId Target;
	int64 BestDist = 0;
	bool bTarget = false;
	const FKodSimEntityState* Lead = States.Find(IdleMarines[0]);
	const FVector From = Lead ? Lead->Position : Base;
	for (int32 Key : StructureKeys)
	{
		const FKodStructureState* Structure = Structures.Find(Key);
		if (!Structure || Structure->TeamId != 0 || !States.Contains(Structure->Id.Value))
		{
			continue;
		}
		const int64 Dist = Dist2DSq(From, Structure->Position);
		if (!bTarget || Dist < BestDist)
		{
			bTarget = true;
			BestDist = Dist;
			Target = Structure->Id;
		}
	}
	if (!Target.IsValid())
	{
		return;
	}

	for (int32 Index = 0; Index < WaveSize && Index < IdleMarines.Num(); ++Index)
	{
		FKodEntityId MarineId;
		MarineId.Value = IdleMarines[Index];
		IssueAttack(MarineId, Target);
	}
	++AiWavesLaunched;
	AiNextWaveTick = SimTickIndex + static_cast<int64>(AiWaveGapTicks);
	UE_LOG(LogTemp, Log, TEXT("KodAI Wave Size=%d Target=%d"), WaveSize, Target.Value);
}
