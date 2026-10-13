#pragma once

#include "CoreMinimal.h"

/**
 * Post-game scoreboard. Sampled every 10 sim seconds.
 * Not part of ComputeIdleHash and not written into the save blob.
 */
struct KODCORE_API FKodScoreTypeRow
{
	FName Id;
	int32 Trained = 0;
	int32 Killed = 0;
	int32 Lost = 0;
};

struct KODCORE_API FKodScoreSample
{
	int32 Tick = 0;
	int32 Workers = 0;
	int32 ArmyValue = 0;
	int32 SupplyUsed = 0;
	int32 SupplyCap = 0;
	int32 Commands = 0;
	int32 JadeiteCollected = 0;
	int32 LumineneCollected = 0;
	int32 JadeiteSpent = 0;
	int32 LumineneSpent = 0;
};

struct KODCORE_API FKodScorePlayer
{
	int32 TeamId = 0;
	int32 ColorIndex = 0;
	int32 Commands = 0;
	int32 JadeiteCollected = 0;
	int32 LumineneCollected = 0;
	int32 JadeiteSpent = 0;
	int32 LumineneSpent = 0;
	int32 UnitsTrained = 0;
	int32 UnitsKilled = 0;
	int32 UnitsLost = 0;
	int32 StructuresBuilt = 0;
	int32 StructuresRazed = 0;
	int32 StructuresLost = 0;
	TArray<FKodScoreTypeRow> Units;
	TArray<FKodScoreTypeRow> Structures;
	TArray<FKodScoreSample> Samples;
};

struct KODCORE_API FKodMatchScoreTracker
{
	/** 10 seconds at SimHz 16. */
	static constexpr int32 SampleTicks = 10 * 16;

	FKodMatchScoreTracker();

	void Reset();
	void SetTeamColor(int32 TeamId, int32 ColorIndex);
	void NoteCommand(int32 TeamId);
	void NoteGather(int32 TeamId, int32 Jadeite, int32 Luminene);
	void NoteSpend(int32 TeamId, int32 Jadeite, int32 Luminene);
	void NoteUnitTrained(int32 TeamId, FName UnitId);
	void NoteUnitKilled(int32 KillerTeam, FName UnitId);
	void NoteUnitLost(int32 OwnerTeam, FName UnitId);
	void NoteStructureBuilt(int32 TeamId, FName TypeId);
	void NoteStructureRazed(int32 KillerTeam, FName TypeId);
	void NoteStructureLost(int32 OwnerTeam, FName TypeId);

	/** Append one sample when this tick was not already stored. */
	void ConsiderSample(int32 Tick, int32 TeamId, int32 Workers, int32 ArmyValue, int32 SupplyUsed, int32 SupplyCap);

	const FKodScorePlayer* FindPlayer(int32 TeamId) const;
	int32 AverageApm(int32 TeamId, int32 SimTick) const;
	int32 PeakApm(int32 TeamId) const;
	static int32 PerMinute(int32 DeltaAmount, int32 DeltaTicks);

	TArray<FKodScorePlayer> Players;
};
