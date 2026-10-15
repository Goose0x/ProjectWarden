#pragma once

#include "CoreMinimal.h"

/**
 * One yaw for every building mesh. The square build-grid footprint is not rotated:
 * a 45 degree mesh would otherwise stick out of its cells. ContainScale shrinks the
 * mesh so its axis-aligned box still matches that square, which keeps the nav blocker
 * and the selection pad on the same cells and leaves no walkable corner outside the mesh.
 * kod.BuildingYaw overrides [KodMeshes] BuildingYawOffset.
 */
namespace KodBuildingYaw
{
	KODCORE_API float OffsetDegrees();

	/** 1 at 0 and 90 degrees. 1/sqrt(2) at 45 degrees, so a fitted square stays inside its footprint. */
	KODCORE_API float ContainScale(float YawDegrees);

	KODCORE_API float PresentationYaw(float RowYaw);
}
