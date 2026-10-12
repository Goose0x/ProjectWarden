#include "Game/KodApmTracker.h"

#include "Engine/World.h"

void UKodApmTracker::NoteCommand()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (StartedAt < 0.f)
	{
		StartedAt = Now;
	}
	CommandTimes.Add(Now);
}

void UKodApmTracker::GetApm(int32& OutAverage, int32& OutPeak) const
{
	OutAverage = 0;
	OutPeak = 0;
	if (CommandTimes.Num() == 0)
	{
		return;
	}
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : CommandTimes.Last();
	const float Minutes = FMath::Max(1.f / 60.f, (Now - StartedAt) / 60.f);
	OutAverage = FMath::RoundToInt(static_cast<float>(CommandTimes.Num()) / Minutes);
	int32 Best = 0;
	int32 Window = 0;
	for (int32 Index = 0; Index < CommandTimes.Num(); ++Index)
	{
		while (Window < Index && CommandTimes[Index] - CommandTimes[Window] > 60.f)
		{
			++Window;
		}
		Best = FMath::Max(Best, Index - Window + 1);
	}
	OutPeak = Best;
}