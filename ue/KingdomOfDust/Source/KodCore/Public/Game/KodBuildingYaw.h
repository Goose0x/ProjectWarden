#pragma once

#include "CoreMinimal.h"

/**
 * Optional debug yaw on every building mesh. Default is 0: the 3D team authors
 * buildings diagonal, and the square grid footprint stays at 0 degrees.
 * A nonzero kod.BuildingYaw turns the mesh at full fitted scale. Corners may
 * overhang the pad. kod.BuildingYaw overrides [KodMeshes] BuildingYawOffset.
 */
namespace KodBuildingYaw
{
	KODCORE_API float OffsetDegrees();
	KODCORE_API float PresentationYaw(float RowYaw);
}
