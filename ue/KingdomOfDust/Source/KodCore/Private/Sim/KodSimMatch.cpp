#include "Sim/KodSimSubsystem.h"

#include "Game/KodSkirmishSaveGame.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	struct FBlob
	{
		TArray<uint8> Bytes;
		int32 Read = 0;

		void W(const void* Data, int32 Size)
		{
			const int32 Offset = Bytes.Num();
			Bytes.AddUninitialized(Size);
			FMemory::Memcpy(Bytes.GetData() + Offset, Data, Size);
		}
		bool R(void* Data, int32 Size)
		{
			if (Read + Size > Bytes.Num())
			{
				return false;
			}
			FMemory::Memcpy(Data, Bytes.GetData() + Read, Size);
			Read += Size;
			return true;
		}
		void WInt(int32 Value) { W(&Value, sizeof(Value)); }
		bool RInt(int32& Value) { return R(&Value, sizeof(Value)); }
		void WU8(uint8 Value) { W(&Value, sizeof(Value)); }
		bool RU8(uint8& Value) { return R(&Value, sizeof(Value)); }
		void WFloat(float Value) { W(&Value, sizeof(Value)); }
		bool RFloat(float& Value) { return R(&Value, sizeof(Value)); }
		void WStr(const FString& Value)
		{
			const int32 Len = Value.Len();
			WInt(Len);
			if (Len > 0)
			{
				W(*Value, Len * sizeof(TCHAR));
			}
		}
		bool RStr(FString& Value)
		{
			int32 Len = 0;
			if (!RInt(Len) || Len < 0 || Len > 256)
			{
				return false;
			}
			Value.Reset(Len);
			if (Len == 0)
			{
				return true;
			}
			TArray<TCHAR> Chars;
			Chars.SetNum(Len + 1);
			if (!R(Chars.GetData(), Len * sizeof(TCHAR)))
			{
				return false;
			}
			Chars[Len] = 0;
			Value = Chars.GetData();
			return true;
		}
	};

}

void UKodSimSubsystem::SetExposedRule(bool bEnabled)
{
	bExposedRule = bEnabled;
	if (!bEnabled)
	{
		bTeam0Exposed = false;
		bTeam1Exposed = false;
	}
	UE_LOG(LogTemp, Log, TEXT("KodExposed Rule=%d"), bEnabled ? 1 : 0);
}

void UKodSimSubsystem::SetExposedTimeSeconds(int32 Seconds)
{
	ExposedTicks = FMath::Max(1, Seconds) * 16;
	UE_LOG(LogTemp, Log, TEXT("KodExposed Time=%d"), Seconds);
}

bool UKodSimSubsystem::IsTeamExposed(int32 TeamId) const
{
	return TeamId == 0 ? bTeam0Exposed : bTeam1Exposed;
}

int32 UKodSimSubsystem::ArmyValueFor(int32 TeamId) const
{
	int32 Value = 0;
	TArray<int32> Keys;
	States.GetKeys(Keys);
	for (int32 Key : Keys)
	{
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->TeamId != TeamId || State->Health <= 0.f || !State->bMobile)
		{
			continue;
		}
		FKodTrainSpec Spec;
		if (TryGetTrainSpec(State->DefinitionId, Spec))
		{
			Value += FMath::Max(0, Spec.Cost.Credits);
		}
	}
	return Value;
}

void UKodSimSubsystem::NoteResources(int32 TeamId, int32 Amount)
{
	(TeamId == 0 ? Totals0 : Totals1).Collected += FMath::Max(0, Amount);
}

void UKodSimSubsystem::NoteUnitTrained(int32 TeamId)
{
	(TeamId == 0 ? Totals0 : Totals1).Trained += 1;
}

void UKodSimSubsystem::NoteUnitKilled(int32 TeamId)
{
	(TeamId == 0 ? Totals0 : Totals1).Killed += 1;
}

void UKodSimSubsystem::NoteStructureBuilt(int32 TeamId)
{
	(TeamId == 0 ? Totals0 : Totals1).Built += 1;
}

void UKodSimSubsystem::NoteStructureRazed(int32 TeamId)
{
	(TeamId == 0 ? Totals0 : Totals1).Razed += 1;
}

void UKodSimSubsystem::GetTeamTotals(int32 TeamId, int32& Collected, int32& Trained, int32& Killed, int32& Built, int32& Razed) const
{
	const FKodTeamTotals& Totals = TeamId == 0 ? Totals0 : Totals1;
	Collected = Totals.Collected;
	Trained = Totals.Trained;
	Killed = Totals.Killed;
	Built = Totals.Built;
	Razed = Totals.Razed;
}

void UKodSimSubsystem::CopyStatSamples(int32 TeamId, TArray<FKodStatSample>& Out) const
{
	Out = TeamId == 0 ? Samples0 : Samples1;
}

void UKodSimSubsystem::CopyOlderStatSamples(int32 TeamId, TArray<FKodStatSample>& Out) const
{
	Out = TeamId == 0 ? OlderSamples0 : OlderSamples1;
}

void UKodSimSubsystem::CollapseStatWindow(TArray<FKodStatSample>& Samples, TArray<FKodStatSample>& Older)
{
	constexpr int32 FullSamples = 30 * 60 / 5;
	constexpr int32 CollapseCount = 12;
	constexpr int32 OlderCap = 512;
	while (Samples.Num() > FullSamples && Samples.Num() >= CollapseCount)
	{
		FKodStatSample Collapsed = Samples[CollapseCount - 1];
		int32 Income = 0;
		for (int32 Index = 0; Index < CollapseCount; ++Index)
		{
			Income += Samples[Index].Income;
		}
		Collapsed.Income = Income;
		Older.Add(Collapsed);
		Samples.RemoveAt(0, CollapseCount);
	}
	while (Older.Num() > OlderCap)
	{
		Older.RemoveAt(0);
	}
}

void UKodSimSubsystem::RecordMatchSample()
{
	if (bTestIsolation || SimTickIndex <= 0 || (SimTickIndex % (5 * 16)) != 0)
	{
		return;
	}
	auto Push = [this](int32 TeamId, FKodTeamTotals& Totals, TArray<FKodStatSample>& Samples)
	{
		FKodStatSample Sample;
		Sample.Tick = static_cast<int32>(SimTickIndex);
		Sample.Collected = Totals.Collected;
		Sample.Trained = Totals.Trained;
		Sample.ArmyValue = ArmyValueFor(TeamId);
		Sample.Income = Totals.Collected - Totals.LastIncomeCollected;
		Sample.Killed = Totals.Killed;
		Sample.Built = Totals.Built;
		Sample.Razed = Totals.Razed;
		Totals.LastIncomeCollected = Totals.Collected;
		Samples.Add(Sample);
	};
	Push(0, Totals0, Samples0);
	Push(1, Totals1, Samples1);
	CollapseStatWindow(Samples0, OlderSamples0);
	CollapseStatWindow(Samples1, OlderSamples1);
}

void UKodSimSubsystem::StepExposed()
{
	if (bTestIsolation || !bExposedRule)
	{
		return;
	}
	const FName CcId = FName(KodWardenPaths::Id_CommandCenter);
	auto CountCc = [this, CcId](int32 TeamId, int32& Any, FVector& Where)
	{
		Any = 0;
		TArray<int32> Keys;
		Structures.GetKeys(Keys);
		Keys.Sort();
		for (int32 Key : Keys)
		{
			const FKodStructureState* Structure = Structures.Find(Key);
			if (!Structure || Structure->TeamId != TeamId || Structure->DefinitionId != CcId)
			{
				continue;
			}
			++Any;
			Where = Structure->Position;
		}
	};
	auto Update = [this, &CountCc](int32 TeamId, bool& bExposed, bool& bWarned)
	{
		int32 Any = 0;
		FVector Where = FVector::ZeroVector;
		CountCc(TeamId, Any, Where);
		if (Any >= 2)
		{
			bExposed = false;
			return;
		}
		if (Any != 1)
		{
			return;
		}
		if (!bWarned && SimTickIndex >= ExposedTicks - 60 * 16)
		{
			bWarned = true;
			RaiseCue(FName(TEXT("expand_or_be_exposed")), Where, false);
			UE_LOG(LogTemp, Log, TEXT("KodExposed Warn Team=%d"), TeamId);
		}
		if (!bExposed && SimTickIndex >= ExposedTicks)
		{
			bExposed = true;
			RaiseCue(FName(TEXT("exposed")), Where, false);
			UE_LOG(LogTemp, Log, TEXT("KodExposed Start Team=%d"), TeamId);
		}
	};
	Update(0, bTeam0Exposed, bTeam0ExposedWarned);
	Update(1, bTeam1Exposed, bTeam1ExposedWarned);
}

void UKodSimSubsystem::ApplyExposedReveal()
{
	if (!bExposedRule)
	{
		return;
	}
	auto Reveal = [this](int32 ExposedTeam)
	{
		if (!IsTeamExposed(ExposedTeam))
		{
			return;
		}
		TArray<int32> Keys;
		Structures.GetKeys(Keys);
		Keys.Sort();
		TArray<int32> Watchers;
		TeamVision.GetKeys(Watchers);
		Watchers.Sort();
		for (int32 Watcher : Watchers)
		{
			if (Watcher == ExposedTeam)
			{
				continue;
			}
			TArray<uint8>* Grid = TeamVision.Find(Watcher);
			if (!Grid)
			{
				continue;
			}
			for (int32 Key : Keys)
			{
				const FKodStructureState* Structure = Structures.Find(Key);
				if (!Structure || Structure->TeamId != ExposedTeam)
				{
					continue;
				}
				for (int32 X = Structure->CellX; X < Structure->CellX + Structure->FootprintX; ++X)
				{
					for (int32 Y = Structure->CellY; Y < Structure->CellY + Structure->FootprintY; ++Y)
					{
						const int32 Index = VisionCellIndex(X, Y);
						if (Grid->IsValidIndex(Index))
						{
							(*Grid)[Index] = 2;
						}
					}
				}
			}
		}
	};
	Reveal(0);
	Reveal(1);
}

void UKodSimSubsystem::ResyncActors() const
{
	for (const TPair<int32, FKodSimEntityState>& Pair : States)
	{
		const TWeakObjectPtr<AActor>* ActorPtr = Entities.Find(Pair.Key);
		AActor* Actor = ActorPtr ? ActorPtr->Get() : nullptr;
		if (!Actor)
		{
			continue;
		}
		Actor->SetActorLocation(Pair.Value.Position);
		FRotator Rot = Actor->GetActorRotation();
		Rot.Yaw = Pair.Value.YawDegrees;
		Actor->SetActorRotation(Rot);
	}
}

TArray<uint8> UKodSimSubsystem::ExportSimBlob() const
{
	FBlob Blob;
	const int32 Version = 4;
	Blob.WInt(Version);
	Blob.W(&SimTickIndex, sizeof(SimTickIndex));
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	Blob.WInt(Keys.Num());
	for (int32 Key : Keys)
	{
		const FKodSimEntityState& S = States.FindChecked(Key);
		Blob.WInt(Key);
		Blob.WStr(S.DefinitionId.ToString());
		Blob.WInt(S.TeamId);
		Blob.WU8(S.bMobile ? 1 : 0);
		Blob.WU8(S.bCanGather ? 1 : 0);
		Blob.WFloat(S.YawDegrees);
		Blob.WInt(FMath::RoundToInt(S.MoveSpeed));
		Blob.WInt(FMath::RoundToInt(S.WeaponDamage));
		Blob.WInt(FMath::RoundToInt(S.WeaponRange));
		Blob.WFloat(S.Position.X);
		Blob.WFloat(S.Position.Y);
		Blob.WFloat(S.Position.Z);
		Blob.WFloat(S.Health);
		Blob.WFloat(S.MaxHealth);
		Blob.WU8(static_cast<uint8>(S.Order));
		Blob.WU8(S.bExplicitAttack ? 1 : 0);
		Blob.WU8(static_cast<uint8>(S.GatherPhase));
		Blob.WInt(S.GatherNode.Value);
		Blob.WInt(S.GatherDropOff.Value);
		Blob.WU8(static_cast<uint8>(S.GatherType));
		Blob.WInt(S.CargoJadeite);
		Blob.WInt(S.CargoLuminene);
		Blob.WU8(S.bCargoFromTrickle ? 1 : 0);
		Blob.WInt(S.HarvestTicksRemaining);
		Blob.WInt(S.GatherStallTicks);
		Blob.WU8(S.bGatherStallRetargeted ? 1 : 0);
		Blob.WInt(S.SupplyCost);
		Blob.WInt(S.WeaponCooldownTicks);
		Blob.WInt(S.CooldownTicksRemaining);
		Blob.WInt(S.BodyRadiusUU);
		Blob.WInt(S.SightRadiusUU);
		Blob.WInt(S.ArmorPoints);
		Blob.WInt(S.Energy);
		Blob.WInt(S.EnergyMax);
		Blob.WInt(S.EnergyRegenAcc);
		Blob.WInt(S.HealAcc);
		Blob.WU8(S.bAutocastHeal ? 1 : 0);
		Blob.WU8(S.bBiological ? 1 : 0);
		Blob.WU8(S.bInfantry ? 1 : 0);
		Blob.WU8(S.bHealer ? 1 : 0);
		Blob.WU8(S.bFlyer ? 1 : 0);
		Blob.WInt(S.CargoSize);
		Blob.WInt(S.InsideBunker.Value);
		Blob.WInt(S.LoadTarget.Value);
		Blob.WInt(S.WeaponBackswingTicks);
		Blob.WInt(S.BackswingTicksRemaining);
		Blob.WInt(S.FollowTarget.Value);
		Blob.WInt(S.PathId);
		Blob.WInt(S.PathCursor);
		Blob.WInt(S.PathOffsetX);
		Blob.WInt(S.PathOffsetY);
		Blob.WFloat(S.PatrolAnchor.X);
		Blob.WFloat(S.PatrolAnchor.Y);
		Blob.WInt(S.OrderQueue.Num());
		for (const FKodQueuedOrder& Step : S.OrderQueue)
		{
			Blob.WU8(static_cast<uint8>(Step.Order));
			Blob.WInt(Step.X);
			Blob.WInt(Step.Y);
			Blob.WInt(Step.TargetId);
			Blob.WInt(Step.PathId);
			Blob.WInt(Step.OffsetX);
			Blob.WInt(Step.OffsetY);
		}
		Blob.WU8(S.bSupplyCounted ? 1 : 0);
		Blob.WInt(S.BuildSite.Value);
		Blob.WStr(S.ActiveAbilityId.ToString());
		Blob.WInt(S.AbilityTicksRemaining);
		Blob.WInt(S.AbilityCooldownTicks);
		Blob.WInt(S.AbilityMovePermille);
		Blob.WInt(S.AbilityAttackPermille);
		Blob.WInt(S.GatherRatePermille > 0 ? S.GatherRatePermille : 1000);
		Blob.WInt(S.OrbitAngle);
		Blob.WInt(S.OrbitCenterX);
		Blob.WInt(S.OrbitCenterY);
		Blob.WInt(S.OrbitRadiusUU);
		Blob.WInt(S.OrbitTicksRemaining);
	}
	TArray<int32> UniqueTeams;
	Banks.GetKeys(UniqueTeams);
	TArray<int32> SupplyKeys;
	Supply.GetKeys(SupplyKeys);
	for (int32 TeamId : SupplyKeys)
	{
		UniqueTeams.AddUnique(TeamId);
	}
	UniqueTeams.Sort();
	Blob.WInt(UniqueTeams.Num());
	for (int32 TeamId : UniqueTeams)
	{
		const FKodResourceCost* Bank = Banks.Find(TeamId);
		const FKodSupplyState* SupplyRow = Supply.Find(TeamId);
		Blob.WInt(TeamId);
		Blob.WInt(Bank ? Bank->Jadeite : 0);
		Blob.WInt(Bank ? Bank->Luminene : 0);
		Blob.WInt(SupplyRow ? SupplyRow->Used : 0);
		Blob.WInt(SupplyRow ? SupplyRow->Cap : 0);
	}
	TArray<int32> NodeKeys;
	ResourceNodes.GetKeys(NodeKeys);
	NodeKeys.Sort();
	Blob.WInt(NodeKeys.Num());
	for (int32 Key : NodeKeys)
	{
		const FKodResourceNodeState& Node = ResourceNodes.FindChecked(Key);
		Blob.WInt(Key);
		Blob.WU8(static_cast<uint8>(Node.Type));
		Blob.WInt(Node.Remaining);
		Blob.WFloat(Node.Position.X);
		Blob.WFloat(Node.Position.Y);
		Blob.WFloat(Node.Position.Z);
	}
	TArray<int32> DropKeys;
	DropOffs.GetKeys(DropKeys);
	DropKeys.Sort();
	Blob.WInt(DropKeys.Num());
	for (int32 Key : DropKeys)
	{
		const FKodDropOffState& Drop = DropOffs.FindChecked(Key);
		Blob.WInt(Key);
		Blob.WInt(Drop.TeamId);
		Blob.WFloat(Drop.Position.X);
		Blob.WFloat(Drop.Position.Y);
		Blob.WFloat(Drop.Position.Z);
	}
	TArray<int32> StructureKeys;
	Structures.GetKeys(StructureKeys);
	StructureKeys.Sort();
	Blob.WInt(StructureKeys.Num());
	for (int32 Key : StructureKeys)
	{
		const FKodStructureState& Structure = Structures.FindChecked(Key);
		Blob.WInt(Key);
		Blob.WStr(Structure.DefinitionId.ToString());
		Blob.WFloat(Structure.Position.X);
		Blob.WFloat(Structure.Position.Y);
		Blob.WFloat(Structure.Position.Z);
		const FKodSimEntityState* Combat = States.Find(Key);
		Blob.WFloat(Combat ? Combat->YawDegrees : 0.f);
		Blob.WInt(Structure.TeamId);
		Blob.WInt(Structure.SupplyProvided);
		Blob.WInt(Structure.Slots.Num());
		for (const FKodProductionSlot& Slot : Structure.Slots)
		{
			Blob.WStr(Slot.UnitId.ToString());
			Blob.WInt(Slot.BuildTime);
			Blob.WU8(Slot.bStarted ? 1 : 0);
		}
		Blob.WInt(Structure.ActiveTicksRemaining);
		Blob.WInt(Structure.CellX);
		Blob.WInt(Structure.CellY);
		Blob.WInt(Structure.FootprintX);
		Blob.WInt(Structure.FootprintY);
		Blob.WInt(Structure.BuildTimeTotal);
		Blob.WInt(Structure.BuildTicksDone);
		Blob.WU8(Structure.bComplete ? 1 : 0);
		Blob.WInt(Structure.BuilderId.Value);
		Blob.WInt(Structure.VentId.Value);
		Blob.WU8(Structure.bSupplyBlocked ? 1 : 0);
		Blob.WInt(Structure.bRallyNode ? Structure.RallyNode.Value : 0);
		Blob.WInt(Structure.RallyUnit.Value);
		Blob.WU8(Structure.bLowered ? 1 : 0);
		Blob.WInt(Structure.CargoCap);
		TArray<int32> Cargo = Structure.Cargo;
		Cargo.Sort();
		Blob.WInt(Cargo.Num());
		for (int32 Rider : Cargo)
		{
			Blob.WInt(Rider);
		}
		Blob.WInt(Structure.Research.Num());
		Blob.WInt(Structure.ResearchTicksRemaining);
		for (const FKodProductionSlot& Job : Structure.Research)
		{
			Blob.WStr(Job.UnitId.ToString());
			Blob.WInt(Job.BuildTime);
		}
		Blob.WFloat(Structure.RallyPoint.X);
		Blob.WFloat(Structure.RallyPoint.Y);
		Blob.WFloat(Structure.RallyPoint.Z);
		Blob.WInt(Structure.BonusSupply);
		Blob.WInt(Structure.BonusSupplyTicksRemaining);
		Blob.WInt(Structure.GathererCd);
		Blob.WInt(Structure.SupplyCd);
		Blob.WInt(Structure.IsrCd);
		Blob.WU8(Structure.bAirborne ? 1 : 0);
		Blob.WU8(Structure.bLiftMoving ? 1 : 0);
		Blob.WInt(Structure.LiftGoalX);
		Blob.WInt(Structure.LiftGoalY);
		Blob.WInt(Structure.LiftSpeedUU);
	}
	Blob.WU8(static_cast<uint8>(MatchOutcome));
	Blob.WU8(bAiEnabled ? 1 : 0);
	Blob.WInt(AiDifficulty);
	Blob.WInt(AiWavesLaunched);
	Blob.W(&AiNextWaveTick, sizeof(AiNextWaveTick));
	Blob.WU8(bAiSeeded ? 1 : 0);
	Blob.WU8(bAiScoutSent ? 1 : 0);
	Blob.WU8(bHasEnemyStart ? 1 : 0);
	Blob.WInt(EnemyStartX);
	Blob.WInt(EnemyStartY);
	TArray<int32> TechTeams;
	TeamTech.GetKeys(TechTeams);
	TechTeams.Sort();
	Blob.WInt(TechTeams.Num());
	for (int32 TeamId : TechTeams)
	{
		const FKodTeamTech& Tech = TeamTech.FindChecked(TeamId);
		Blob.WInt(TeamId);
		Blob.WInt(Tech.InfantryWeapons);
		Blob.WInt(Tech.InfantryArmor);
		Blob.WInt(Tech.SatelliteFeed);
	}
	TArray<int32> FacingTeams;
	TeamFacingYaw.GetKeys(FacingTeams);
	FacingTeams.Sort();
	Blob.WInt(FacingTeams.Num());
	for (int32 TeamId : FacingTeams)
	{
		Blob.WInt(TeamId);
		Blob.WInt(TeamFacingYaw.FindChecked(TeamId));
	}
	Blob.WU8(bAiCheatVision ? 1 : 0);
	Blob.WInt(VisionOriginX);
	Blob.WInt(VisionOriginY);
	Blob.WInt(VisionWidth);
	Blob.WInt(VisionHeight);
	TArray<int32> VisionTeams;
	TeamVision.GetKeys(VisionTeams);
	VisionTeams.Sort();
	Blob.WInt(VisionTeams.Num());
	for (int32 TeamId : VisionTeams)
	{
		Blob.WInt(TeamId);
		const TArray<uint8>& Grid = TeamVision.FindChecked(TeamId);
		Blob.WInt(Grid.Num());
		if (Grid.Num() > 0)
		{
			Blob.W(Grid.GetData(), Grid.Num());
		}
		const TArray<FKodSeenStructure>* Seen = SeenStructures.Find(TeamId);
		Blob.WInt(Seen ? Seen->Num() : 0);
		if (Seen)
		{
			for (const FKodSeenStructure& Entry : *Seen)
			{
				Blob.WInt(Entry.Id);
				Blob.WInt(Entry.TeamId);
				Blob.WStr(Entry.DefinitionId.ToString());
				Blob.WInt(Entry.PosX);
				Blob.WInt(Entry.PosY);
				Blob.WInt(Entry.FootprintX);
				Blob.WInt(Entry.FootprintY);
				Blob.WInt(Entry.Health);
				Blob.WInt(Entry.MaxHealth);
			}
		}
		const TArray<FKodSeenUnit>* SeenMobile = SeenUnits.Find(TeamId);
		Blob.WInt(SeenMobile ? SeenMobile->Num() : 0);
		if (SeenMobile)
		{
			for (const FKodSeenUnit& Entry : *SeenMobile)
			{
				Blob.WInt(Entry.Id);
				Blob.WInt(Entry.TeamId);
				Blob.WInt(Entry.PosX);
				Blob.WInt(Entry.PosY);
			}
		}
	}
	Blob.WU8(bExposedRule ? 1 : 0);
	Blob.WInt(ExposedTicks);
	Blob.WU8(bTeam0Exposed ? 1 : 0);
	Blob.WU8(bTeam1Exposed ? 1 : 0);
	Blob.WU8(bTeam0ExposedWarned ? 1 : 0);
	Blob.WU8(bTeam1ExposedWarned ? 1 : 0);
	auto WriteTotals = [&Blob](const FKodTeamTotals& Totals)
	{
		Blob.WInt(Totals.Collected);
		Blob.WInt(Totals.Trained);
		Blob.WInt(Totals.Killed);
		Blob.WInt(Totals.Built);
		Blob.WInt(Totals.Razed);
		Blob.WInt(Totals.LastIncomeCollected);
	};
	WriteTotals(Totals0);
	WriteTotals(Totals1);
	auto WriteSamples = [&Blob](const TArray<FKodStatSample>& Samples)
	{
		Blob.WInt(Samples.Num());
		for (const FKodStatSample& Sample : Samples)
		{
			Blob.WInt(Sample.Tick);
			Blob.WInt(Sample.Collected);
			Blob.WInt(Sample.Trained);
			Blob.WInt(Sample.ArmyValue);
			Blob.WInt(Sample.Income);
			Blob.WInt(Sample.Killed);
			Blob.WInt(Sample.Built);
			Blob.WInt(Sample.Razed);
		}
	};
	WriteSamples(Samples0);
	WriteSamples(Samples1);
	WriteSamples(OlderSamples0);
	WriteSamples(OlderSamples1);
	Blob.WU8(bMatchCheated ? 1 : 0);
	Blob.WU8(bCompetitive ? 1 : 0);
	Blob.WU8(bCheatInvincible ? 1 : 0);
	Blob.WU8(bCheatInstantBuild ? 1 : 0);
	Blob.WU8(bCheatNoSupply ? 1 : 0);
	Blob.WU8(bCheatReveal ? 1 : 0);
	Blob.WU8(bCheatNoCooldown ? 1 : 0);
	Blob.WInt(DamageDealt0);
	Blob.WInt(DamageDealt1);
	Blob.WInt(DamageTaken0);
	Blob.WInt(DamageTaken1);
	Blob.WInt(AiOpener);
	Blob.WU8(bAiOpenerChosen ? 1 : 0);
	Blob.WU8(bScoutSawEnemy ? 1 : 0);
	Blob.WU8(bAiRetreating ? 1 : 0);
	Blob.WU8(bGeneralPowers ? 1 : 0);
	Blob.WInt(PowerPoints0);
	Blob.WInt(PowerPoints1);
	Blob.WInt(ArtilleryCd0);
	Blob.WInt(ArtilleryCd1);
	Blob.WInt(ReconCd0);
	Blob.WInt(ReconCd1);
	Blob.WInt(RepairCd0);
	Blob.WInt(RepairCd1);
	Blob.WInt(ScheduledPowers.Num());
	for (const FKodScheduledPower& Power : ScheduledPowers)
	{
		Blob.WInt(Power.TeamId);
		Blob.WStr(Power.PowerId.ToString());
		Blob.WInt(Power.X);
		Blob.WInt(Power.Y);
		Blob.WInt(Power.TicksLeft);
		Blob.WInt(Power.Phase);
		Blob.WInt(Power.RadiusUU);
		Blob.WInt(Power.Amount);
		Blob.WInt(Power.DurationTicks);
	}
	Blob.WInt(RampCells.Num());
	for (int64 Ramp : RampCells)
	{
		Blob.W(&Ramp, sizeof(Ramp));
	}
	return Blob.Bytes;
}

void UKodSimSubsystem::ImportSimBlob(const TArray<uint8>& Bytes)
{
	FBlob Blob;
	Blob.Bytes = Bytes;
	int32 Version = 0;
	if (!Blob.RInt(Version) || Version != 4)
	{
		return;
	}
	Blob.R(&SimTickIndex, sizeof(SimTickIndex));
	int32 EntityCount = 0;
	Blob.RInt(EntityCount);
	TSet<int32> SeenEntityIds;
	for (int32 Index = 0; Index < EntityCount; ++Index)
	{
		int32 Key = 0;
		FString Name;
		Blob.RInt(Key);
		Blob.RStr(Name);
		int32 TeamId = 0;
		uint8 Mobile = 1;
		uint8 Gather = 0;
		float Yaw = 0.f;
		int32 MoveSpeed = 0;
		int32 WeaponDamage = 0;
		int32 WeaponRange = 0;
		Blob.RInt(TeamId);
		Blob.RU8(Mobile);
		Blob.RU8(Gather);
		Blob.RFloat(Yaw);
		Blob.RInt(MoveSpeed);
		Blob.RInt(WeaponDamage);
		Blob.RInt(WeaponRange);
		FKodSimEntityState& Added = States.FindOrAdd(Key);
		FKodSimEntityState* S = &Added;
		S->Id.Value = Key;
		SeenEntityIds.Add(Key);
		float X = 0.f;
		float Y = 0.f;
		float Z = 0.f;
		Blob.RFloat(X);
		Blob.RFloat(Y);
		Blob.RFloat(Z);
		float Health = 0.f;
		float MaxHealth = 0.f;
		Blob.RFloat(Health);
		Blob.RFloat(MaxHealth);
		uint8 OrderByte = 0;
		uint8 Explicit = 0;
		Blob.RU8(OrderByte);
		Blob.RU8(Explicit);
		uint8 GatherPhase = 0;
		int32 GatherNode = 0;
		int32 GatherDrop = 0;
		uint8 GatherType = 0;
		Blob.RU8(GatherPhase);
		Blob.RInt(GatherNode);
		Blob.RInt(GatherDrop);
		Blob.RU8(GatherType);
		int32 CargoJ = 0;
		int32 CargoL = 0;
		uint8 Trickle = 0;
		int32 Harvest = 0;
		int32 Stall = 0;
		uint8 StallRetarget = 0;
		int32 SupplyCost = 0;
		int32 WeaponCd = 0;
		int32 CdLeft = 0;
		Blob.RInt(CargoJ);
		Blob.RInt(CargoL);
		Blob.RU8(Trickle);
		Blob.RInt(Harvest);
		Blob.RInt(Stall);
		Blob.RU8(StallRetarget);
		Blob.RInt(SupplyCost);
		Blob.RInt(WeaponCd);
		Blob.RInt(CdLeft);
		int32 Body = 0;
		int32 Sight = 0;
		int32 Armor = 0;
		int32 Energy = 0;
		int32 EnergyMax = 0;
		int32 Regen = 0;
		int32 Heal = 0;
		Blob.RInt(Body);
		Blob.RInt(Sight);
		Blob.RInt(Armor);
		Blob.RInt(Energy);
		Blob.RInt(EnergyMax);
		Blob.RInt(Regen);
		Blob.RInt(Heal);
		uint8 Autocast = 0;
		uint8 Biological = 0;
		uint8 Infantry = 0;
		uint8 Healer = 0;
		uint8 Flyer = 0;
		Blob.RU8(Autocast);
		Blob.RU8(Biological);
		Blob.RU8(Infantry);
		Blob.RU8(Healer);
		Blob.RU8(Flyer);
		int32 CargoSize = 0;
		int32 Inside = 0;
		int32 Load = 0;
		int32 Backswing = 0;
		int32 BackLeft = 0;
		int32 Follow = 0;
		int32 PathId = 0;
		int32 PathCursor = 0;
		int32 OffX = 0;
		int32 OffY = 0;
		Blob.RInt(CargoSize);
		Blob.RInt(Inside);
		Blob.RInt(Load);
		Blob.RInt(Backswing);
		Blob.RInt(BackLeft);
		Blob.RInt(Follow);
		Blob.RInt(PathId);
		Blob.RInt(PathCursor);
		Blob.RInt(OffX);
		Blob.RInt(OffY);
		float PatrolX = 0.f;
		float PatrolY = 0.f;
		Blob.RFloat(PatrolX);
		Blob.RFloat(PatrolY);
		int32 QueueCount = 0;
		Blob.RInt(QueueCount);
		TArray<FKodQueuedOrder> Queue;
		for (int32 StepIndex = 0; StepIndex < QueueCount; ++StepIndex)
		{
			FKodQueuedOrder Step;
			uint8 Kind = 0;
			Blob.RU8(Kind);
			Step.Order = static_cast<EKodSimOrderType>(Kind);
			Blob.RInt(Step.X);
			Blob.RInt(Step.Y);
			Blob.RInt(Step.TargetId);
			Blob.RInt(Step.PathId);
			Blob.RInt(Step.OffsetX);
			Blob.RInt(Step.OffsetY);
			Queue.Add(Step);
		}
		uint8 SupplyCounted = 0;
		int32 BuildSite = 0;
		FString Ability;
		Blob.RU8(SupplyCounted);
		Blob.RInt(BuildSite);
		Blob.RStr(Ability);
		int32 AbilityTicks = 0;
		int32 AbilityCd = 0;
		int32 MovePermille = 0;
		int32 AttackPermille = 0;
		Blob.RInt(AbilityTicks);
		Blob.RInt(AbilityCd);
		Blob.RInt(MovePermille);
		Blob.RInt(AttackPermille);
		S->DefinitionId = FName(*Name);
		S->TeamId = TeamId;
		S->bMobile = Mobile != 0;
		S->bCanGather = Gather != 0;
		S->YawDegrees = Yaw;
		S->MoveSpeed = static_cast<float>(MoveSpeed);
		S->WeaponDamage = static_cast<float>(WeaponDamage);
		S->WeaponRange = static_cast<float>(WeaponRange);
		S->Position = FVector(X, Y, Z);
		S->Health = Health;
		S->MaxHealth = MaxHealth;
		S->Order = static_cast<EKodSimOrderType>(OrderByte);
		S->bExplicitAttack = Explicit != 0;
		S->GatherPhase = static_cast<EKodGatherPhase>(GatherPhase);
		S->GatherNode.Value = GatherNode;
		S->GatherDropOff.Value = GatherDrop;
		S->GatherType = static_cast<EKodResourceType>(GatherType);
		S->CargoJadeite = CargoJ;
		S->CargoLuminene = CargoL;
		S->bCargoFromTrickle = Trickle != 0;
		S->HarvestTicksRemaining = Harvest;
		S->GatherStallTicks = Stall;
		S->bGatherStallRetargeted = StallRetarget != 0;
		S->SupplyCost = SupplyCost;
		S->WeaponCooldownTicks = WeaponCd;
		S->CooldownTicksRemaining = CdLeft;
		S->BodyRadiusUU = Body;
		S->SightRadiusUU = Sight;
		S->ArmorPoints = Armor;
		S->Energy = Energy;
		S->EnergyMax = EnergyMax;
		S->EnergyRegenAcc = Regen;
		S->HealAcc = Heal;
		S->bAutocastHeal = Autocast != 0;
		S->bBiological = Biological != 0;
		S->bInfantry = Infantry != 0;
		S->bHealer = Healer != 0;
		S->bFlyer = Flyer != 0;
		S->CargoSize = CargoSize;
		S->InsideBunker.Value = Inside;
		S->LoadTarget.Value = Load;
		S->WeaponBackswingTicks = Backswing;
		S->BackswingTicksRemaining = BackLeft;
		S->FollowTarget.Value = Follow;
		S->PathId = PathId;
		S->PathCursor = PathCursor;
		S->PathOffsetX = OffX;
		S->PathOffsetY = OffY;
		S->PatrolAnchor.X = PatrolX;
		S->PatrolAnchor.Y = PatrolY;
		S->OrderQueue = Queue;
		S->bSupplyCounted = SupplyCounted != 0;
		S->BuildSite.Value = BuildSite;
		S->ActiveAbilityId = FName(*Ability);
		S->AbilityTicksRemaining = AbilityTicks;
		S->AbilityCooldownTicks = AbilityCd;
		S->AbilityMovePermille = MovePermille;
		S->AbilityAttackPermille = AttackPermille;
		int32 GatherRate = 1000;
		int32 OrbitAngle = 0;
		int32 OrbitX = 0;
		int32 OrbitY = 0;
		int32 OrbitRadius = 0;
		int32 OrbitTicks = 0;
		Blob.RInt(GatherRate);
		Blob.RInt(OrbitAngle);
		Blob.RInt(OrbitX);
		Blob.RInt(OrbitY);
		Blob.RInt(OrbitRadius);
		Blob.RInt(OrbitTicks);
		S->GatherRatePermille = GatherRate > 0 ? GatherRate : 1000;
		S->OrbitAngle = OrbitAngle;
		S->OrbitCenterX = OrbitX;
		S->OrbitCenterY = OrbitY;
		S->OrbitRadiusUU = OrbitRadius;
		S->OrbitTicksRemaining = OrbitTicks;
	}
	{
		TArray<int32> StateKeys;
		States.GetKeys(StateKeys);
		for (int32 Key : StateKeys)
		{
			if (!SeenEntityIds.Contains(Key))
			{
				FKodEntityId Id;
				Id.Value = Key;
				AActor* Actor = ResolveEntity(Id);
				UnregisterEntity(Id);
				if (Actor && !Actor->IsActorBeingDestroyed())
				{
					Actor->Destroy();
				}
			}
		}
	}
	int32 TeamCount = 0;
	Blob.RInt(TeamCount);
	for (int32 Index = 0; Index < TeamCount; ++Index)
	{
		int32 TeamId = 0;
		int32 Jade = 0;
		int32 Lum = 0;
		int32 Used = 0;
		int32 Cap = 0;
		Blob.RInt(TeamId);
		Blob.RInt(Jade);
		Blob.RInt(Lum);
		Blob.RInt(Used);
		Blob.RInt(Cap);
		FKodResourceCost& Bank = Banks.FindOrAdd(TeamId);
		Bank.Jadeite = Jade;
		Bank.Luminene = Lum;
		FKodSupplyState& SupplyRow = Supply.FindOrAdd(TeamId);
		SupplyRow.Used = Used;
		SupplyRow.Cap = Cap;
	}
	int32 NodeCount = 0;
	Blob.RInt(NodeCount);
	TSet<int32> SeenNodes;
	for (int32 Index = 0; Index < NodeCount; ++Index)
	{
		int32 Key = 0;
		uint8 TypeByte = 0;
		int32 Remaining = 0;
		float X = 0.f;
		float Y = 0.f;
		float Z = 0.f;
		Blob.RInt(Key);
		Blob.RU8(TypeByte);
		Blob.RInt(Remaining);
		Blob.RFloat(X);
		Blob.RFloat(Y);
		Blob.RFloat(Z);
		FKodResourceNodeState& Node = ResourceNodes.FindOrAdd(Key);
		Node.Id.Value = Key;
		Node.Type = static_cast<EKodResourceType>(TypeByte);
		Node.Remaining = Remaining;
		Node.Position = FVector(X, Y, Z);
		SeenNodes.Add(Key);
	}
	{
		TArray<int32> NodeKeys;
		ResourceNodes.GetKeys(NodeKeys);
		for (int32 Key : NodeKeys)
		{
			if (!SeenNodes.Contains(Key))
			{
				ResourceNodes.Remove(Key);
			}
		}
	}
	int32 DropCount = 0;
	Blob.RInt(DropCount);
	TSet<int32> SeenDrops;
	for (int32 Index = 0; Index < DropCount; ++Index)
	{
		int32 Key = 0;
		int32 TeamId = 0;
		float X = 0.f;
		float Y = 0.f;
		float Z = 0.f;
		Blob.RInt(Key);
		Blob.RInt(TeamId);
		Blob.RFloat(X);
		Blob.RFloat(Y);
		Blob.RFloat(Z);
		FKodDropOffState& Drop = DropOffs.FindOrAdd(Key);
		Drop.Id.Value = Key;
		Drop.TeamId = TeamId;
		Drop.Position = FVector(X, Y, Z);
		SeenDrops.Add(Key);
	}
	{
		TArray<int32> DropKeys;
		DropOffs.GetKeys(DropKeys);
		for (int32 Key : DropKeys)
		{
			if (!SeenDrops.Contains(Key))
			{
				DropOffs.Remove(Key);
			}
		}
	}
	int32 StructureCount = 0;
	Blob.RInt(StructureCount);
	TSet<int32> SeenStructureIds;
	for (int32 Index = 0; Index < StructureCount; ++Index)
	{
		int32 Key = 0;
		Blob.RInt(Key);
		FString StructDef;
		float StructX = 0.f;
		float StructY = 0.f;
		float StructZ = 0.f;
		float StructYaw = 0.f;
		Blob.RStr(StructDef);
		Blob.RFloat(StructX);
		Blob.RFloat(StructY);
		Blob.RFloat(StructZ);
		Blob.RFloat(StructYaw);
		FKodStructureState& AddedStructure = Structures.FindOrAdd(Key);
		FKodStructureState* Structure = &AddedStructure;
		Structure->Id.Value = Key;
		SeenStructureIds.Add(Key);
		Structure->DefinitionId = FName(*StructDef);
		Structure->Position = FVector(StructX, StructY, StructZ);
		if (FKodSimEntityState* Combat = States.Find(Key))
		{
			Combat->YawDegrees = StructYaw;
		}
		int32 TeamId = 0;
		int32 SupplyProvided = 0;
		int32 SlotCount = 0;
		Blob.RInt(TeamId);
		Blob.RInt(SupplyProvided);
		Blob.RInt(SlotCount);
		TArray<FKodProductionSlot> Slots;
		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			FKodProductionSlot Slot;
			FString UnitName;
			Blob.RStr(UnitName);
			Slot.UnitId = FName(*UnitName);
			Blob.RInt(Slot.BuildTime);
			uint8 Started = 0;
			Blob.RU8(Started);
			Slot.bStarted = Started != 0;
			Slots.Add(Slot);
		}
		int32 Active = 0;
		int32 CellX = 0;
		int32 CellY = 0;
		int32 FootX = 0;
		int32 FootY = 0;
		int32 BuildTotal = 0;
		int32 BuildDone = 0;
		uint8 Complete = 0;
		int32 Builder = 0;
		int32 Vent = 0;
		uint8 Blocked = 0;
		int32 RallyNode = 0;
		int32 RallyUnit = 0;
		uint8 Lowered = 0;
		int32 CargoCap = 0;
		Blob.RInt(Active);
		Blob.RInt(CellX);
		Blob.RInt(CellY);
		Blob.RInt(FootX);
		Blob.RInt(FootY);
		Blob.RInt(BuildTotal);
		Blob.RInt(BuildDone);
		Blob.RU8(Complete);
		Blob.RInt(Builder);
		Blob.RInt(Vent);
		Blob.RU8(Blocked);
		Blob.RInt(RallyNode);
		Blob.RInt(RallyUnit);
		Blob.RU8(Lowered);
		Blob.RInt(CargoCap);
		int32 CargoCount = 0;
		Blob.RInt(CargoCount);
		TArray<int32> Cargo;
		for (int32 CargoIndex = 0; CargoIndex < CargoCount; ++CargoIndex)
		{
			int32 Rider = 0;
			Blob.RInt(Rider);
			Cargo.Add(Rider);
		}
		int32 ResearchCount = 0;
		int32 ResearchLeft = 0;
		Blob.RInt(ResearchCount);
		Blob.RInt(ResearchLeft);
		TArray<FKodProductionSlot> Research;
		for (int32 ResearchIndex = 0; ResearchIndex < ResearchCount; ++ResearchIndex)
		{
			FKodProductionSlot Job;
			FString UpgradeName;
			Blob.RStr(UpgradeName);
			Job.UnitId = FName(*UpgradeName);
			Blob.RInt(Job.BuildTime);
			Research.Add(Job);
		}
		float RallyX = 0.f;
		float RallyY = 0.f;
		float RallyZ = 0.f;
		Blob.RFloat(RallyX);
		Blob.RFloat(RallyY);
		Blob.RFloat(RallyZ);
		Structure->TeamId = TeamId;
		Structure->SupplyProvided = SupplyProvided;
		Structure->Slots = Slots;
		Structure->ActiveTicksRemaining = Active;
		Structure->CellX = CellX;
		Structure->CellY = CellY;
		Structure->FootprintX = FootX;
		Structure->FootprintY = FootY;
		Structure->BuildTimeTotal = BuildTotal;
		Structure->BuildTicksDone = BuildDone;
		Structure->bComplete = Complete != 0;
		Structure->BuilderId.Value = Builder;
		Structure->VentId.Value = Vent;
		Structure->bSupplyBlocked = Blocked != 0;
		Structure->bRallyNode = RallyNode != 0;
		Structure->RallyNode.Value = RallyNode;
		Structure->RallyUnit.Value = RallyUnit;
		Structure->bLowered = Lowered != 0;
		Structure->CargoCap = CargoCap;
		Structure->Cargo = Cargo;
		Structure->ResearchTicksRemaining = ResearchLeft;
		Structure->Research = Research;
		Structure->RallyPoint = FVector(RallyX, RallyY, RallyZ);
		Blob.RInt(Structure->BonusSupply);
		Blob.RInt(Structure->BonusSupplyTicksRemaining);
		Blob.RInt(Structure->GathererCd);
		Blob.RInt(Structure->SupplyCd);
		Blob.RInt(Structure->IsrCd);
		uint8 Airborne = 0;
		uint8 LiftMoving = 0;
		Blob.RU8(Airborne);
		Blob.RU8(LiftMoving);
		Blob.RInt(Structure->LiftGoalX);
		Blob.RInt(Structure->LiftGoalY);
		Blob.RInt(Structure->LiftSpeedUU);
		Structure->bAirborne = Airborne != 0;
		Structure->bLiftMoving = LiftMoving != 0;
	}
	{
		TArray<int32> StructureKeys;
		Structures.GetKeys(StructureKeys);
		for (int32 Key : StructureKeys)
		{
			if (!SeenStructureIds.Contains(Key))
			{
				Structures.Remove(Key);
			}
		}
	}
	uint8 Outcome = 0;
	uint8 AiOn = 0;
	Blob.RU8(Outcome);
	Blob.RU8(AiOn);
	MatchOutcome = static_cast<EKodMatchOutcome>(Outcome);
	bAiEnabled = AiOn != 0;
	Blob.RInt(AiDifficulty);
	Blob.RInt(AiWavesLaunched);
	Blob.R(&AiNextWaveTick, sizeof(AiNextWaveTick));
	uint8 Seeded = 0;
	uint8 Scout = 0;
	uint8 HasStart = 0;
	Blob.RU8(Seeded);
	Blob.RU8(Scout);
	Blob.RU8(HasStart);
	bAiSeeded = Seeded != 0;
	bAiScoutSent = Scout != 0;
	bHasEnemyStart = HasStart != 0;
	Blob.RInt(EnemyStartX);
	Blob.RInt(EnemyStartY);
	int32 TechCount = 0;
	Blob.RInt(TechCount);
	TeamTech.Reset();
	for (int32 Index = 0; Index < TechCount; ++Index)
	{
		int32 TeamId = 0;
		FKodTeamTech Tech;
		Blob.RInt(TeamId);
		Blob.RInt(Tech.InfantryWeapons);
		Blob.RInt(Tech.InfantryArmor);
		Blob.RInt(Tech.SatelliteFeed);
		TeamTech.Add(TeamId, Tech);
	}
	int32 FacingCount = 0;
	Blob.RInt(FacingCount);
	TeamFacingYaw.Reset();
	for (int32 Index = 0; Index < FacingCount; ++Index)
	{
		int32 TeamId = 0;
		int32 Yaw = 0;
		Blob.RInt(TeamId);
		Blob.RInt(Yaw);
		TeamFacingYaw.Add(TeamId, Yaw);
	}
	uint8 Cheat = 0;
	Blob.RU8(Cheat);
	bAiCheatVision = Cheat != 0;
	Blob.RInt(VisionOriginX);
	Blob.RInt(VisionOriginY);
	Blob.RInt(VisionWidth);
	Blob.RInt(VisionHeight);
	int32 VisionCount = 0;
	Blob.RInt(VisionCount);
	for (int32 Index = 0; Index < VisionCount; ++Index)
	{
		int32 TeamId = 0;
		int32 CellCount = 0;
		Blob.RInt(TeamId);
		Blob.RInt(CellCount);
		TArray<uint8>& Grid = TeamVision.FindOrAdd(TeamId);
		Grid.SetNum(CellCount);
		if (CellCount > 0)
		{
			Blob.R(Grid.GetData(), CellCount);
		}
		int32 SeenCount = 0;
		Blob.RInt(SeenCount);
		TArray<FKodSeenStructure>& Seen = SeenStructures.FindOrAdd(TeamId);
		Seen.Reset();
		for (int32 SeenIndex = 0; SeenIndex < SeenCount; ++SeenIndex)
		{
			FKodSeenStructure Entry;
			FString DefName;
			Blob.RInt(Entry.Id);
			Blob.RInt(Entry.TeamId);
			Blob.RStr(DefName);
			Entry.DefinitionId = FName(*DefName);
			Blob.RInt(Entry.PosX);
			Blob.RInt(Entry.PosY);
			Blob.RInt(Entry.FootprintX);
			Blob.RInt(Entry.FootprintY);
			Blob.RInt(Entry.Health);
			Blob.RInt(Entry.MaxHealth);
			Seen.Add(Entry);
		}
		int32 UnitSeen = 0;
		Blob.RInt(UnitSeen);
		TArray<FKodSeenUnit>& Mobile = SeenUnits.FindOrAdd(TeamId);
		Mobile.Reset();
		for (int32 SeenIndex = 0; SeenIndex < UnitSeen; ++SeenIndex)
		{
			FKodSeenUnit Entry;
			Blob.RInt(Entry.Id);
			Blob.RInt(Entry.TeamId);
			Blob.RInt(Entry.PosX);
			Blob.RInt(Entry.PosY);
			Mobile.Add(Entry);
		}
	}
	uint8 Rule = 0;
	uint8 Ex0 = 0;
	uint8 Ex1 = 0;
	uint8 Warn0 = 0;
	uint8 Warn1 = 0;
	Blob.RU8(Rule);
	Blob.RInt(ExposedTicks);
	Blob.RU8(Ex0);
	Blob.RU8(Ex1);
	Blob.RU8(Warn0);
	Blob.RU8(Warn1);
	bExposedRule = Rule != 0;
	bTeam0Exposed = Ex0 != 0;
	bTeam1Exposed = Ex1 != 0;
	bTeam0ExposedWarned = Warn0 != 0;
	bTeam1ExposedWarned = Warn1 != 0;
	auto ReadTotals = [&Blob](FKodTeamTotals& Totals)
	{
		Blob.RInt(Totals.Collected);
		Blob.RInt(Totals.Trained);
		Blob.RInt(Totals.Killed);
		Blob.RInt(Totals.Built);
		Blob.RInt(Totals.Razed);
		Blob.RInt(Totals.LastIncomeCollected);
	};
	ReadTotals(Totals0);
	ReadTotals(Totals1);
	auto ReadSamples = [&Blob](TArray<FKodStatSample>& Samples)
	{
		int32 Count = 0;
		Blob.RInt(Count);
		Samples.Reset();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FKodStatSample Sample;
			Blob.RInt(Sample.Tick);
			Blob.RInt(Sample.Collected);
			Blob.RInt(Sample.Trained);
			Blob.RInt(Sample.ArmyValue);
			Blob.RInt(Sample.Income);
			Blob.RInt(Sample.Killed);
			Blob.RInt(Sample.Built);
			Blob.RInt(Sample.Razed);
			Samples.Add(Sample);
		}
	};
	ReadSamples(Samples0);
	ReadSamples(Samples1);
	ReadSamples(OlderSamples0);
	ReadSamples(OlderSamples1);
	uint8 Cheated = 0;
	uint8 Competitive = 0;
	uint8 Invincible = 0;
	uint8 Instant = 0;
	uint8 NoSupply = 0;
	uint8 Reveal = 0;
	uint8 NoCooldown = 0;
	Blob.RU8(Cheated);
	Blob.RU8(Competitive);
	Blob.RU8(Invincible);
	Blob.RU8(Instant);
	Blob.RU8(NoSupply);
	Blob.RU8(Reveal);
	Blob.RU8(NoCooldown);
	bMatchCheated = Cheated != 0;
	bCompetitive = Competitive != 0;
	bCheatInvincible = Invincible != 0;
	bCheatInstantBuild = Instant != 0;
	bCheatNoSupply = NoSupply != 0;
	bCheatReveal = Reveal != 0;
	bCheatNoCooldown = NoCooldown != 0;
	Blob.RInt(DamageDealt0);
	Blob.RInt(DamageDealt1);
	Blob.RInt(DamageTaken0);
	Blob.RInt(DamageTaken1);
	Blob.RInt(AiOpener);
	uint8 OpenerChosen = 0;
	uint8 ScoutSaw = 0;
	uint8 Retreating = 0;
	Blob.RU8(OpenerChosen);
	Blob.RU8(ScoutSaw);
	Blob.RU8(Retreating);
	bAiOpenerChosen = OpenerChosen != 0;
	bScoutSawEnemy = ScoutSaw != 0;
	bAiRetreating = Retreating != 0;
	uint8 PowersOn = 0;
	Blob.RU8(PowersOn);
	bGeneralPowers = PowersOn != 0;
	Blob.RInt(PowerPoints0);
	Blob.RInt(PowerPoints1);
	Blob.RInt(ArtilleryCd0);
	Blob.RInt(ArtilleryCd1);
	Blob.RInt(ReconCd0);
	Blob.RInt(ReconCd1);
	Blob.RInt(RepairCd0);
	Blob.RInt(RepairCd1);
	int32 PowerCount = 0;
	Blob.RInt(PowerCount);
	ScheduledPowers.Reset();
	for (int32 Index = 0; Index < PowerCount; ++Index)
	{
		FKodScheduledPower Power;
		FString PowerName;
		Blob.RInt(Power.TeamId);
		Blob.RStr(PowerName);
		Power.PowerId = FName(*PowerName);
		Blob.RInt(Power.X);
		Blob.RInt(Power.Y);
		Blob.RInt(Power.TicksLeft);
		Blob.RInt(Power.Phase);
		Blob.RInt(Power.RadiusUU);
		Blob.RInt(Power.Amount);
		Blob.RInt(Power.DurationTicks);
		ScheduledPowers.Add(Power);
	}
	int32 RampCount = 0;
	Blob.RInt(RampCount);
	RampCells.Reset();
	for (int32 Index = 0; Index < RampCount; ++Index)
	{
		int64 Ramp = 0;
		Blob.R(&Ramp, sizeof(Ramp));
		RampCells.Add(Ramp);
	}
	int32 MaxId = 0;
	TArray<int32> LiveKeys;
	States.GetKeys(LiveKeys);
	for (int32 Key : LiveKeys)
	{
		MaxId = FMath::Max(MaxId, Key);
	}
	NextId = FMath::Max(NextId, MaxId + 1);
}

bool UKodSimSubsystem::SaveSkirmishSlot(int32 Slot)
{
	const int32 UseSlot = FMath::Clamp(Slot, 1, 3);
	const FString Before = ComputeIdleHash();
	UKodSkirmishSaveGame* Save = NewObject<UKodSkirmishSaveGame>();
	Save->Blob = ExportSimBlob();
	Save->StateHash = Before;
	Save->Slot = UseSlot;
	const FString Name = FString::Printf(TEXT("KodSkirmish_%d"), UseSlot);
	const bool bOk = UGameplayStatics::SaveGameToSlot(Save, Name, 0);
	UE_LOG(LogTemp, Log, TEXT("KodSave Slot=%d HashBefore=%s Ok=%d"), UseSlot, *Before, bOk ? 1 : 0);
	return bOk;
}

bool UKodSimSubsystem::CollectSavedEntityIds(const TArray<uint8>& Bytes, TSet<int32>& OutIds) const
{
	FBlob Blob;
	Blob.Bytes = Bytes;
	int32 Version = 0;
	if (!Blob.RInt(Version) || Version != 4)
	{
		return false;
	}
	int64 Tick = 0;
	if (!Blob.R(&Tick, sizeof(Tick)))
	{
		return false;
	}
	int32 EntityCount = 0;
	if (!Blob.RInt(EntityCount) || EntityCount < 0 || EntityCount > 100000)
	{
		return false;
	}
	auto SkipInts = [&Blob](int32 Count) -> bool
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			int32 Value = 0;
			if (!Blob.RInt(Value))
			{
				return false;
			}
		}
		return true;
	};
	auto SkipFloats = [&Blob](int32 Count) -> bool
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			float Value = 0.f;
			if (!Blob.RFloat(Value))
			{
				return false;
			}
		}
		return true;
	};
	auto SkipBytes = [&Blob](int32 Count) -> bool
	{
		for (int32 Index = 0; Index < Count; ++Index)
		{
			uint8 Value = 0;
			if (!Blob.RU8(Value))
			{
				return false;
			}
		}
		return true;
	};
	for (int32 Index = 0; Index < EntityCount; ++Index)
	{
		int32 Key = 0;
		FString Name;
		if (!Blob.RInt(Key) || !Blob.RStr(Name))
		{
			return false;
		}
		OutIds.Add(Key);
		int32 TeamId = 0;
		if (!Blob.RInt(TeamId) || !SkipBytes(2) || !SkipFloats(1) || !SkipInts(3) || !SkipFloats(5))
		{
			return false;
		}
		if (!SkipBytes(2) || !SkipBytes(1) || !SkipInts(2) || !SkipBytes(1) || !SkipInts(2) || !SkipBytes(1))
		{
			return false;
		}
		if (!SkipInts(2) || !SkipBytes(1) || !SkipInts(10) || !SkipBytes(5) || !SkipInts(10) || !SkipFloats(2))
		{
			return false;
		}
		int32 QueueCount = 0;
		if (!Blob.RInt(QueueCount) || QueueCount < 0 || QueueCount > 64)
		{
			return false;
		}
		for (int32 Step = 0; Step < QueueCount; ++Step)
		{
			if (!SkipBytes(1) || !SkipInts(6))
			{
				return false;
			}
		}
		FString Ability;
		if (!SkipBytes(1) || !SkipInts(1) || !Blob.RStr(Ability) || !SkipInts(10))
		{
			return false;
		}
	}
	return true;
}

void UKodSimSubsystem::DestroyEntitiesNotIn(const TSet<int32>& Keep)
{
	TArray<int32> Live;
	Entities.GetKeys(Live);
	Live.Sort();
	for (int32 Key : Live)
	{
		if (Keep.Contains(Key))
		{
			continue;
		}
		FKodEntityId Id;
		Id.Value = Key;
		AActor* Actor = ResolveEntity(Id);
		UnregisterEntity(Id);
		if (Actor && !Actor->IsActorBeingDestroyed())
		{
			Actor->Destroy();
		}
	}
	TArray<int32> StateKeys;
	States.GetKeys(StateKeys);
	for (int32 Key : StateKeys)
	{
		if (Keep.Contains(Key))
		{
			continue;
		}
		FKodEntityId Id;
		Id.Value = Key;
		UnregisterEntity(Id);
	}
}

void UKodSimSubsystem::QueueMissingActors()
{
	TArray<int32> Keys;
	States.GetKeys(Keys);
	Keys.Sort();
	for (int32 Key : Keys)
	{
		FKodEntityId Id;
		Id.Value = Key;
		if (ResolveEntity(Id))
		{
			continue;
		}
		const FKodSimEntityState* State = States.Find(Key);
		if (!State || State->Health <= 0.f)
		{
			continue;
		}
		if (const FKodStructureState* Structure = Structures.Find(Key))
		{
			FKodPendingBuildingSpawn Spawn;
			Spawn.StructureId = Key;
			Spawn.RestoreId = Key;
			Spawn.TypeId = Structure->DefinitionId;
			Spawn.TeamId = Structure->TeamId;
			Spawn.Location = Structure->Position;
			Spawn.FootprintX = FMath::Max(1, Structure->FootprintX);
			Spawn.FootprintY = FMath::Max(1, Structure->FootprintY);
			Spawn.YawDegrees = FMath::RoundToInt(State->YawDegrees);
			PendingBuildingSpawns.Add(Spawn);
			continue;
		}
		if (!State->bMobile)
		{
			continue;
		}
		FKodPendingUnitSpawn Spawn;
		Spawn.RestoreId = Key;
		Spawn.UnitId = State->DefinitionId;
		Spawn.TeamId = State->TeamId;
		Spawn.SupplyCost = State->SupplyCost;
		Spawn.Location = State->Position;
		Spawn.Yaw = State->YawDegrees;
		PendingSpawns.Add(Spawn);
	}
}

void UKodSimSubsystem::AttachRestoredActor(FKodEntityId Id, AActor* Actor)
{
	if (!Id.IsValid() || !Actor)
	{
		return;
	}
	Entities.Add(Id.Value, Actor);
	NextId = FMath::Max(NextId, Id.Value + 1);
}

void UKodSimSubsystem::RestoreImportedState(FKodEntityId Id, const FKodSimEntityState& State)
{
	FKodSimEntityState& Live = States.FindOrAdd(Id.Value);
	Live = State;
	Live.Id = Id;
	PrevPositions.Add(Id.Value, Live.Position);
	PrevYaws.Add(Id.Value, Live.YawDegrees);
}

bool UKodSimSubsystem::LoadSkirmishSlot(int32 Slot)
{
	const int32 UseSlot = FMath::Clamp(Slot, 1, 3);
	const FString Name = FString::Printf(TEXT("KodSkirmish_%d"), UseSlot);
	USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(Name, 0);
	UKodSkirmishSaveGame* Save = Cast<UKodSkirmishSaveGame>(Loaded);
	if (!Save)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodLoad Slot=%d Missing"), UseSlot);
		return false;
	}
	TSet<int32> Keep;
	if (!CollectSavedEntityIds(Save->Blob, Keep))
	{
		UE_LOG(LogTemp, Warning, TEXT("KodLoad Slot=%d BadBlob"), UseSlot);
		return false;
	}
	DestroyEntitiesNotIn(Keep);
	ImportSimBlob(Save->Blob);
	bLoadRespawn = true;
	LoadRespawnTicks = 0;
	TArray<int32> BoundKeys;
	Entities.GetKeys(BoundKeys);
	BoundKeys.Sort();
	TArray<AActor*> BoundActors;
	for (int32 Key : BoundKeys)
	{
		FKodEntityId Id;
		Id.Value = Key;
		if (AActor* Actor = ResolveEntity(Id))
		{
			BoundActors.Add(Actor);
		}
	}
	Entities.Reset();
	for (AActor* Actor : BoundActors)
	{
		if (Actor && !Actor->IsActorBeingDestroyed())
		{
			Actor->Destroy();
		}
	}
	QueueMissingActors();
	ResyncActors();
	const FString After = ComputeIdleHash();
	UE_LOG(LogTemp, Log, TEXT("KodLoad Slot=%d HashBefore=%s HashAfter=%s"), UseSlot, *Save->StateHash, *After);
	return Save->StateHash == After;
}
