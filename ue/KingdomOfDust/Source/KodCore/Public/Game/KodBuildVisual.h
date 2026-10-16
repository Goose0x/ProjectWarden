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
}
