#pragma once

#include "CoreMinimal.h"

/** One chosen line. bPlay is false when the unit type is still talking. */
struct KODCORE_API FKodVoiceChoice
{
	FName Cue;
	FName Category;
	bool bPlay = false;
};

/**
 * SC2-style unit voice.
 * Select, Ack, and Attack are no-repeat shuffle bags.
 * Four selects on the same unit inside the window walk the Annoyed lines in order.
 * A missing Annoyed bag falls back to the Select shuffle.
 * One unit type speaks at a time. The cooldown depends on the category.
 */
class KODCORE_API FKodVoiceDirector
{
public:
	void AddLine(FName UnitId, FName Category, FName Cue);
	static bool IsAltTake(FName Cue);
	bool HasCategory(FName UnitId, FName Category) const;
	FKodVoiceChoice Choose(FName UnitId, FName Category, int32 EntityId, float NowSeconds);

	float SelectCooldown = 1.5f;
	float AckCooldown = 0.8f;
	/** Annoyed clips run about 4-5.7s. The next select waits until this ends. */
	float AnnoyedCooldown = 5.7f;
	float AnnoyedWindow = 3.f;
	int32 AnnoyedAfter = 4;

private:
	struct FBag
	{
		TArray<FName> Lines;
		TArray<int32> Order;
		int32 Cursor = 0;
		FName Last;
	};

	struct FClicks
	{
		int32 Count = 0;
		int32 AnnoyedIndex = 0;
		float LastSeconds = -1000.f;
		bool bAnnoyedLine = false;
	};

	FName Key(FName UnitId, FName Category) const;
	FName NextShuffled(FName UnitId, FName Category);
	float CooldownFor(FName Category) const;

	TMap<FName, FBag> Bags;
	TMap<int32, FClicks> Clicks;
	TMap<FName, float> BusyUntil;
	FRandomStream Stream;
	bool bSeeded = false;
};
