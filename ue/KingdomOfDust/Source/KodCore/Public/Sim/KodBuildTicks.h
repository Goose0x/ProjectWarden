#pragma once

#include "CoreMinimal.h"

/**
 * Slice 0 / Warden sim clock helpers.
 * Law: BuildTicks = RoundToInt(Seconds * SimHz), SimHz = 16.
 */
namespace KodBuildTicks
{
	/** Authoritative sim rate for Slice 0 (Hz). */
	constexpr int32 SimHz = 16;

	/** Fixed sim step in seconds (1/16). */
	constexpr float SimDt = 1.f / 16.f;

	/** Max catch-up steps per frame to avoid spiral-of-death. */
	constexpr int32 MaxCatchUpSteps = 4;

	/** Convert wall/authoring seconds to integer build ticks. */
	FORCEINLINE int32 SecondsToBuildTicks(float Seconds)
	{
		return FMath::RoundToInt(Seconds * static_cast<float>(SimHz));
	}

	/** Convert build ticks back to seconds (presentation / UI). */
	FORCEINLINE float BuildTicksToSeconds(int32 Ticks)
	{
		return static_cast<float>(Ticks) / static_cast<float>(SimHz);
	}

	/**
	 * Enqueue helper: prefer explicit BuildTicks; else convert BuildTimeSeconds.
	 * Returns at least 1 tick when Seconds > 0 so a positive authoring time never rounds to idle.
	 */
	FORCEINLINE int32 ResolveBuildTicks(int32 BuildTicks, float BuildTimeSeconds)
	{
		if (BuildTicks > 0)
		{
			return BuildTicks;
		}
		if (BuildTimeSeconds <= 0.f)
		{
			return 0;
		}
		return FMath::Max(1, SecondsToBuildTicks(BuildTimeSeconds));
	}
}
