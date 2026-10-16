#pragma once

#include "CoreMinimal.h"

/**
 * How a building is shown while BuildTicks are still running.
 * Slice buildings (Vanguard and Scinder) may swap three stage meshes.
 * Every other building stays on a data-driven placeholder: a scaffold that
 * scales up, or build VFX only. The dynasty ghost hook is off by default.
 */
enum class EKodBuildVisual : uint8
{
	Scaffold,
	Vfx,
	Stages
};

namespace KodBuildVisual
{
	KODCORE_API bool IsSliceBuilding(FName Id);
	KODCORE_API EKodBuildVisual ParseMode(const FString& Text);
	/** Stages are kept only for a slice building that actually has the three meshes. */
	KODCORE_API EKodBuildVisual Resolve(FName BuildingId, EKodBuildVisual Requested, bool bHasStageMeshes);
	KODCORE_API float Progress(int32 TicksDone, int32 TicksTotal);
	/** 0, 1, 2 while building. 3 is the final mesh. Thresholds are kod.Build.Stage2 and Stage3. */
	KODCORE_API int32 StageIndex(float Progress);
	/** kod.Build.DynastyGhost. 0 for the Oct 20 demo. The hook stays in the building. */
	KODCORE_API bool DynastyGhostEnabled();

	/** RiteWell pool. Stage 0..3 maps to fill 0, 0.33, 0.66, 1. */
	inline float StageFillLevel(int32 Stage)
	{
		switch (FMath::Clamp(Stage, 0, 3))
		{
		case 0: return 0.f;
		case 1: return 0.33f;
		case 2: return 0.66f;
		default: return 1.f;
		}
	}

	/** Pool surface relative Z in centimeters for that same stage. */
	inline float PoolSurfaceZcm(int32 Stage)
	{
		switch (FMath::Clamp(Stage, 0, 3))
		{
		case 0: return -160.f;
		case 1: return -42.f;
		case 2: return -10.f;
		default: return 0.f;
		}
	}

	inline constexpr float PoolZInterpSeconds = 0.5f;

	/** Visual-only slide from one pool height to the next. */
	inline float InterpPoolZ(float FromZ, float ToZ, float ElapsedSeconds, float DurationSeconds = PoolZInterpSeconds)
	{
		if (DurationSeconds <= KINDA_SMALL_NUMBER)
		{
			return ToZ;
		}
		const float Alpha = FMath::Clamp(ElapsedSeconds / DurationSeconds, 0.f, 1.f);
		return FMath::Lerp(FromZ, ToZ, Alpha);
	}
}
