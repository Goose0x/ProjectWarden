#pragma once

#include "CoreMinimal.h"
#include "Sim/KodBuildTypes.h"

/**
 * Director SC2 camera defaults, in SC2 game units except the angles.
 * 1 SC2 cell equals one build cell. SupplyDepot is 2x2 and KodBuildGrid::CellUU
 * is 200, so the depot is 400 cm and KodCameraUnitScale is 200 uu per SC2 unit.
 * Gameplay distance rests at 34. Wheel zoom may come in to 16 and stays there.
 * kod.Cam.Distance is the cvar.
 */
namespace KodCameraSettings
{
	inline constexpr float PitchDegrees = -56.f;
	inline constexpr float YawDegrees = 90.f;
	inline constexpr float RollDegrees = 0.f;
	inline constexpr float FovDegrees = 35.f;
	/** SC2 units. 34 * 200 uu is the locked resting arm (6800). */
	inline constexpr float DistanceSc2 = 34.f;
	inline constexpr float DistanceMinSc2 = 16.f;
	inline constexpr float TargetZSc2 = 0.f;
	inline constexpr float NearSc2 = 0.1f;
	inline constexpr float FarSc2 = 150.f;
	inline constexpr float ZoomStepSc2 = 2.f;
	inline constexpr float ZoomReturnSpeed = 1.75f;
	inline constexpr int32 DepotFootprintCells = 2;
	inline constexpr int32 HqFootprintCells = 5;
	/** Standing worker height in uu. */
	inline constexpr float WorkerBodyUU = 242.f;
	/** Standing marine height in uu. */
	inline constexpr float MarineBodyUU = 286.f;

	/** uu per SC2 game unit. One build cell. */
	inline constexpr float UnitScaleUU = static_cast<float>(KodBuildGrid::CellUU);

	inline constexpr float ToUU(float Sc2Units, float ScaleUU = UnitScaleUU)
	{
		return Sc2Units * ScaleUU;
	}

	/** SC2 near/far (0.1 / 150) in Unreal units. 150 uu would clip past 1.5 m. */
	inline void ResolveClipUU(float InNearSc2, float InFarSc2, float ScaleUU, float& OutNearUU, float& OutFarUU)
	{
		const float Scale = ScaleUU > 0.f ? ScaleUU : UnitScaleUU;
		OutNearUU = FMath::Max(0.01f, ToUU(InNearSc2, Scale));
		OutFarUU = FMath::Max(OutNearUU + 1.f, ToUU(InFarSc2, Scale));
	}

	inline constexpr float DepotSpanUU()
	{
		return UnitScaleUU * static_cast<float>(DepotFootprintCells);
	}

	inline constexpr float HqSpanUU()
	{
		return UnitScaleUU * static_cast<float>(HqFootprintCells);
	}

	/** Horizontal field of view from the vertical fov and the viewport aspect. */
	inline float HorizontalFovDegrees(float VerticalFov, float Aspect)
	{
		const float HalfVertical = FMath::DegreesToRadians(FMath::Max(1.f, VerticalFov) * 0.5f);
		const float HalfHorizontal = FMath::Atan(FMath::Tan(HalfVertical) * FMath::Max(0.1f, Aspect));
		return FMath::RadiansToDegrees(HalfHorizontal * 2.f);
	}

	/**
	 * Width of a ground edge at the look-at point, as a fraction of the viewport width.
	 * The edge is measured along camera-right, so pitch does not foreshorten it.
	 */
	inline float ScreenWidthFraction(float WorldWidthUU, float ArmUU, float VerticalFov, float Aspect)
	{
		const float HalfHorizontal = FMath::DegreesToRadians(HorizontalFovDegrees(VerticalFov, Aspect) * 0.5f);
		const float Visible = 2.f * FMath::Max(1.f, ArmUU) * FMath::Tan(HalfHorizontal);
		return WorldWidthUU / FMath::Max(1.f, Visible);
	}

	/** On-screen pixels of a vertical body at the look-at point. Pitch foreshortens world up. */
	inline float BodyPixels(float BodyUU, float ArmUU, float VerticalFov, float Pitch, float ViewHeightPx)
	{
		const float HalfVertical = FMath::Tan(FMath::DegreesToRadians(FMath::Max(1.f, VerticalFov) * 0.5f));
		const float Perpendicular = BodyUU * FMath::Cos(FMath::DegreesToRadians(FMath::Abs(Pitch)));
		const float Fraction = (Perpendicular / FMath::Max(1.f, ArmUU)) / (2.f * HalfVertical);
		return Fraction * ViewHeightPx;
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

	/**
	 * The play plane never follows the sample. Bumps and cliffs stay on LockedZ.
	 * StepUU remains so older call sites still compile. It does not move the plane.
	 */
	inline float HoldPlaneZ(float LockedZ, float SampledZ, float StepUU)
	{
		(void)SampledZ;
		(void)StepUU;
		return LockedZ;
	}

	/** Screen or world bounds of four view corners. A trapezoid becomes a rectangle. */
	struct FKodMapRect
	{
		float MinX = 0.f;
		float MinY = 0.f;
		float MaxX = 0.f;
		float MaxY = 0.f;
		bool bValid = false;
	};

	inline FKodMapRect AxisAlignedBounds(float X0, float Y0, float X1, float Y1, float X2, float Y2, float X3, float Y3)
	{
		FKodMapRect Rect;
		Rect.MinX = FMath::Min(FMath::Min(X0, X1), FMath::Min(X2, X3));
		Rect.MaxX = FMath::Max(FMath::Max(X0, X1), FMath::Max(X2, X3));
		Rect.MinY = FMath::Min(FMath::Min(Y0, Y1), FMath::Min(Y2, Y3));
		Rect.MaxY = FMath::Max(FMath::Max(Y0, Y1), FMath::Max(Y2, Y3));
		Rect.bValid = Rect.MaxX > Rect.MinX && Rect.MaxY > Rect.MinY;
		return Rect;
	}

	inline FKodMapRect ClampRect(const FKodMapRect& Rect, float MinX, float MinY, float MaxX, float MaxY)
	{
		FKodMapRect Out = Rect;
		Out.MinX = FMath::Clamp(Rect.MinX, MinX, MaxX);
		Out.MaxX = FMath::Clamp(Rect.MaxX, MinX, MaxX);
		Out.MinY = FMath::Clamp(Rect.MinY, MinY, MaxY);
		Out.MaxY = FMath::Clamp(Rect.MaxY, MinY, MaxY);
		Out.bValid = Out.MaxX > Out.MinX && Out.MaxY > Out.MinY;
		return Out;
	}

	/**
	 * Spring-arm offset from the focus to the camera. The arm looks along
	 * (Pitch, Yaw) and the socket sits behind that look, so the screen center
	 * ray hits the focus. TargetOffset and SocketOffset stay zero.
	 */
	inline FVector RtsCameraOffset(float Pitch, float Yaw, float ArmUU)
	{
		return -FRotator(Pitch, Yaw, 0.f).Vector() * ArmUU;
	}

	/** Where the view-center ray meets a horizontal plane. False if it aims away. */
	inline bool RayHitsHorizontalPlane(const FVector& Origin, const FVector& Direction, float PlaneZ, FVector& OutHit)
	{
		const FVector Dir = Direction.GetSafeNormal();
		if (FMath::Abs(Dir.Z) < 0.001f)
		{
			return false;
		}
		const float Distance = (PlaneZ - Origin.Z) / Dir.Z;
		if (Distance <= 0.f)
		{
			return false;
		}
		OutHit = Origin + Dir * Distance;
		return true;
	}

	/**
	 * Edge pan snaps to the requested velocity. Releasing the keys or the edge
	 * zeroes it on that step, so it does not coast. Keyboard accel cannot pass the target.
	 */
	inline FVector IntegratePanVelocity(const FVector& Current, const FVector& Desired, float DeltaSeconds, float AccelPerSecond, bool bInstant)
	{
		if (bInstant || Desired.IsNearlyZero())
		{
			return Desired;
		}
		if (DeltaSeconds <= 0.f || AccelPerSecond <= KINDA_SMALL_NUMBER)
		{
			return Desired;
		}
		return FMath::VInterpConstantTo(Current, Desired, DeltaSeconds, AccelPerSecond);
	}

	/** Kept for the ground-sample call. The plane no longer steps, so this does not move the camera. */
	inline constexpr float PlaneHoldUU = 64.f;
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
