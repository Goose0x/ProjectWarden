#pragma once

#include "CoreMinimal.h"
#include "Sim/KodBuildTypes.h"

/**
 * Director SC2 camera defaults, in SC2 game units except the angles.
 * 1 SC2 cell equals one build cell. SupplyDepot is 2x2 and KodBuildGrid::CellUU
 * is 200, so the depot is 400 cm and KodCameraUnitScale is 200 uu per SC2 unit.
 * Gameplay distance rests at 34. Wheel zoom may come in to 16, then eases back.
 */
namespace KodCameraSettings
{
	inline constexpr float PitchDegrees = -56.f;
	inline constexpr float YawDegrees = 90.f;
	inline constexpr float RollDegrees = 0.f;
	inline constexpr float FovDegrees = 35.f;
	inline constexpr float DistanceSc2 = 34.f;
	inline constexpr float DistanceMinSc2 = 16.f;
	inline constexpr float TargetZSc2 = 0.f;
	inline constexpr float NearSc2 = 0.1f;
	inline constexpr float FarSc2 = 150.f;
	inline constexpr float ZoomStepSc2 = 2.f;
	inline constexpr float ZoomReturnSpeed = 1.75f;
	inline constexpr int32 DepotFootprintCells = 2;

	/** uu per SC2 game unit. One build cell. */
	inline constexpr float UnitScaleUU = static_cast<float>(KodBuildGrid::CellUU);

	inline constexpr float ToUU(float Sc2Units, float ScaleUU = UnitScaleUU)
	{
		return Sc2Units * ScaleUU;
	}

	inline constexpr float DepotSpanUU()
	{
		return UnitScaleUU * static_cast<float>(DepotFootprintCells);
	}

	/** Gameplay lock: no closer than the minimum, no farther than the resting distance. */
	inline float ClampGameplayDistanceSc2(float Distance, float MinDistance, float LockedDistance)
	{
		return FMath::Clamp(Distance, MinDistance, LockedDistance);
	}

	/** Move Current toward the locked distance. Alpha 0 stays, 1 arrives. */
	inline float EaseDistanceSc2(float Current, float LockedDistance, float Alpha)
	{
		const float T = Alpha < 0.f ? 0.f : (Alpha > 1.f ? 1.f : Alpha);
		return Current + (LockedDistance - Current) * T;
	}
}

/** Live kod.Cam.* values. Defaults match KodCameraSettings. */
struct KODCORE_API FKodCameraTunables
{
	float Pitch = KodCameraSettings::PitchDegrees;
	float Yaw = KodCameraSettings::YawDegrees;
	float Roll = KodCameraSettings::RollDegrees;
	float Fov = KodCameraSettings::FovDegrees;
	float DistanceSc2 = KodCameraSettings::DistanceSc2;
	float DistanceMinSc2 = KodCameraSettings::DistanceMinSc2;
	float TargetZSc2 = KodCameraSettings::TargetZSc2;
	float NearSc2 = KodCameraSettings::NearSc2;
	float FarSc2 = KodCameraSettings::FarSc2;
	float ZoomStepSc2 = KodCameraSettings::ZoomStepSc2;
	float ZoomReturnSpeed = KodCameraSettings::ZoomReturnSpeed;
	float UnitScaleUU = KodCameraSettings::UnitScaleUU;

	static FKodCameraTunables Read();
	FString DescribeScale() const;
};
