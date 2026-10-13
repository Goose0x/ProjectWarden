#include "Sim/KodScoreTracker.h"

namespace
{
	FKodScoreTypeRow& FindScoreRow(TArray<FKodScoreTypeRow>& Rows, FName TypeId)
	{
		for (FKodScoreTypeRow& Existing : Rows)
		{
			if (Existing.Id == TypeId)
			{
				return Existing;
			}
		}
		FKodScoreTypeRow Added;
		Added.Id = TypeId;
		Rows.Add(Added);
		Rows.Sort([](const FKodScoreTypeRow& A, const FKodScoreTypeRow& B)
		{
			return A.Id.ToString() < B.Id.ToString();
		});
		for (FKodScoreTypeRow& Existing : Rows)
		{
			if (Existing.Id == TypeId)
			{
				return Existing;
			}
		}
		return Rows[0];
	}
}

FKodMatchScoreTracker::FKodMatchScoreTracker()
{
	Reset();
}

void FKodMatchScoreTracker::Reset()
{
	Players.Reset();
	FKodScorePlayer You;
	You.TeamId = 0;
	You.ColorIndex = 0;
	FKodScorePlayer Opponent;
	Opponent.TeamId = 1;
	Opponent.ColorIndex = 1;
	Players.Add(You);
	Players.Add(Opponent);
}

const FKodScorePlayer* FKodMatchScoreTracker::FindPlayer(int32 TeamId) const
{
	for (const FKodScorePlayer& Player : Players)
	{
		if (Player.TeamId == TeamId)
		{
			return &Player;
		}
	}
	return nullptr;
}

namespace
{
	FKodScorePlayer* FindMutablePlayer(TArray<FKodScorePlayer>& Players, int32 TeamId)
	{
		for (FKodScorePlayer& Player : Players)
		{
			if (Player.TeamId == TeamId)
			{
				return &Player;
			}
		}
		return nullptr;
	}
}

void FKodMatchScoreTracker::SetTeamColor(int32 TeamId, int32 ColorIndex)
{
	if (FKodScorePlayer* Player = FindMutablePlayer(Players, TeamId))
	{
		Player->ColorIndex = FMath::Clamp(ColorIndex, 0, 5);
	}
}

void FKodMatchScoreTracker::NoteCommand(int32 TeamId)
{
	if (FKodScorePlayer* Player = FindMutablePlayer(Players, TeamId))
	{
		++Player->Commands;
	}
}

void FKodMatchScoreTracker::NoteGather(int32 TeamId, int32 Jadeite, int32 Luminene)
{
	FKodScorePlayer* Player = FindMutablePlayer(Players, TeamId);
	if (!Player)
	{
		return;
	}
	Player->JadeiteCollected += FMath::Max(0, Jadeite);
	Player->LumineneCollected += FMath::Max(0, Luminene);
}

void FKodMatchScoreTracker::NoteSpend(int32 TeamId, int32 Jadeite, int32 Luminene)
{
	FKodScorePlayer* Player = FindMutablePlayer(Players, TeamId);
	if (!Player)
	{
		return;
	}
	Player->JadeiteSpent += FMath::Max(0, Jadeite);
	Player->LumineneSpent += FMath::Max(0, Luminene);
}

void FKodMatchScoreTracker::NoteUnitTrained(int32 TeamId, FName UnitId)
{
	FKodScorePlayer* Player = FindMutablePlayer(Players, TeamId);
	if (!Player)
	{
		return;
	}
	++Player->UnitsTrained;
	++FindScoreRow(Player->Units, UnitId).Trained;
}

void FKodMatchScoreTracker::NoteUnitKilled(int32 KillerTeam, FName UnitId)
{
	FKodScorePlayer* Player = FindMutablePlayer(Players, KillerTeam);
	if (!Player)
	{
		return;
	}
	++Player->UnitsKilled;
	++FindScoreRow(Player->Units, UnitId).Killed;
}

void FKodMatchScoreTracker::NoteUnitLost(int32 OwnerTeam, FName UnitId)
{
	FKodScorePlayer* Player = FindMutablePlayer(Players, OwnerTeam);
	if (!Player)
	{
		return;
	}
	++Player->UnitsLost;
	++FindScoreRow(Player->Units, UnitId).Lost;
}

void FKodMatchScoreTracker::NoteStructureBuilt(int32 TeamId, FName TypeId)
{
	FKodScorePlayer* Player = FindMutablePlayer(Players, TeamId);
	if (!Player)
	{
		return;
	}
	++Player->StructuresBuilt;
	++FindScoreRow(Player->Structures, TypeId).Trained;
}

void FKodMatchScoreTracker::NoteStructureRazed(int32 KillerTeam, FName TypeId)
{
	FKodScorePlayer* Player = FindMutablePlayer(Players, KillerTeam);
	if (!Player)
	{
		return;
	}
	++Player->StructuresRazed;
	++FindScoreRow(Player->Structures, TypeId).Killed;
}

void FKodMatchScoreTracker::NoteStructureLost(int32 OwnerTeam, FName TypeId)
{
	FKodScorePlayer* Player = FindMutablePlayer(Players, OwnerTeam);
	if (!Player)
	{
		return;
	}
	++Player->StructuresLost;
	++FindScoreRow(Player->Structures, TypeId).Lost;
}

void FKodMatchScoreTracker::ConsiderSample(int32 Tick, int32 TeamId, int32 Workers, int32 ArmyValue, int32 SupplyUsed, int32 SupplyCap)
{
	FKodScorePlayer* Player = FindMutablePlayer(Players, TeamId);
	if (!Player)
	{
		return;
	}
	if (Player->Samples.Num() > 0 && Player->Samples.Last().Tick == Tick)
	{
		return;
	}
	FKodScoreSample Sample;
	Sample.Tick = Tick;
	Sample.Workers = FMath::Max(0, Workers);
	Sample.ArmyValue = FMath::Max(0, ArmyValue);
	Sample.SupplyUsed = FMath::Max(0, SupplyUsed);
	Sample.SupplyCap = FMath::Max(0, SupplyCap);
	Sample.Commands = Player->Commands;
	Sample.JadeiteCollected = Player->JadeiteCollected;
	Sample.LumineneCollected = Player->LumineneCollected;
	Sample.JadeiteSpent = Player->JadeiteSpent;
	Sample.LumineneSpent = Player->LumineneSpent;
	Player->Samples.Add(Sample);
}

int32 FKodMatchScoreTracker::AverageApm(int32 TeamId, int32 SimTick) const
{
	const FKodScorePlayer* Player = FindPlayer(TeamId);
	if (!Player || SimTick <= 0)
	{
		return 0;
	}
	return static_cast<int32>((static_cast<int64>(Player->Commands) * 960) / static_cast<int64>(SimTick));
}

int32 FKodMatchScoreTracker::PeakApm(int32 TeamId) const
{
	const FKodScorePlayer* Player = FindPlayer(TeamId);
	if (!Player || Player->Samples.Num() == 0)
	{
		return 0;
	}
	int32 Best = 0;
	for (int32 Index = 0; Index < Player->Samples.Num(); ++Index)
	{
		const FKodScoreSample& Newer = Player->Samples[Index];
		int32 OlderCommands = 0;
		int32 OlderTick = 0;
		if (Index >= 6)
		{
			OlderCommands = Player->Samples[Index - 6].Commands;
			OlderTick = Player->Samples[Index - 6].Tick;
		}
		const int32 DeltaCommands = Newer.Commands - OlderCommands;
		const int32 DeltaTicks = FMath::Max(1, Newer.Tick - OlderTick);
		const int32 Rate = static_cast<int32>((static_cast<int64>(DeltaCommands) * 960) / static_cast<int64>(DeltaTicks));
		Best = FMath::Max(Best, Rate);
	}
	return Best;
}

int32 FKodMatchScoreTracker::PerMinute(int32 DeltaAmount, int32 DeltaTicks)
{
	if (DeltaTicks <= 0)
	{
		return 0;
	}
	return static_cast<int32>((static_cast<int64>(FMath::Max(0, DeltaAmount)) * 960) / static_cast<int64>(DeltaTicks));
}
