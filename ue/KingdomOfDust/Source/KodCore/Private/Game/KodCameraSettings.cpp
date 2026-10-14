#include "Game/KodCameraSettings.h"

#include "HAL/IConsoleManager.h"

namespace
{
	TAutoConsoleVariable<float> CVarCamPitch(
		TEXT("kod.Cam.Pitch"),
		KodCameraSettings::PitchDegrees,
		TEXT("Camera pitch in degrees. Negative looks down."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamYaw(
		TEXT("kod.Cam.Yaw"),
		KodCameraSettings::YawDegrees,
		TEXT("Camera yaw in degrees. 90 puts map north at the top of the screen."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamRoll(
		TEXT("kod.Cam.Roll"),
		KodCameraSettings::RollDegrees,
		TEXT("Camera roll in degrees."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamFov(
		TEXT("kod.Cam.Fov"),
		KodCameraSettings::FovDegrees,
		TEXT("Vertical field of view in degrees. The pawn converts it to horizontal for the viewport."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamDistance(
		TEXT("kod.Cam.Distance"),
		KodCameraSettings::DistanceSc2,
		TEXT("Resting spring-arm distance in SC2 game units. Gameplay is locked here."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamDistanceMin(
		TEXT("kod.Cam.DistanceMin"),
		KodCameraSettings::DistanceMinSc2,
		TEXT("Closest wheel-zoom distance in SC2 game units. Gameplay eases back to kod.Cam.Distance."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamTargetZ(
		TEXT("kod.Cam.TargetZ"),
		KodCameraSettings::TargetZSc2,
		TEXT("Look-at Z offset from the terrain base, in SC2 game units."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamNear(
		TEXT("kod.Cam.Near"),
		KodCameraSettings::NearSc2,
		TEXT("Perspective near clip in SC2 game units."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamFar(
		TEXT("kod.Cam.Far"),
		KodCameraSettings::FarSc2,
		TEXT("Far clip in SC2 game units. Converted with kod.Cam.UnitScale (150 -> 30000 uu). Never applied as raw uu."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamZoomStep(
		TEXT("kod.Cam.ZoomStep"),
		KodCameraSettings::ZoomStepSc2,
		TEXT("SC2 game units moved closer per mouse-wheel notch."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamZoomReturn(
		TEXT("kod.Cam.ZoomReturn"),
		KodCameraSettings::ZoomReturnSpeed,
		TEXT("How fast gameplay zoom eases back to kod.Cam.Distance."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarCamUnitScale(
		TEXT("kod.Cam.UnitScale"),
		KodCameraSettings::UnitScaleUU,
		TEXT("Unreal units per SC2 game unit. Default is one build cell (SupplyDepot 2x2 = 400 cm)."),
		ECVF_Default);
}

FKodCameraTunables FKodCameraTunables::Read()
{
	FKodCameraTunables Tunables;
	Tunables.Pitch = CVarCamPitch.GetValueOnGameThread();
	Tunables.Yaw = CVarCamYaw.GetValueOnGameThread();
	Tunables.Roll = CVarCamRoll.GetValueOnGameThread();
	Tunables.Fov = CVarCamFov.GetValueOnGameThread();
	Tunables.DistanceSc2 = CVarCamDistance.GetValueOnGameThread();
	Tunables.DistanceMinSc2 = CVarCamDistanceMin.GetValueOnGameThread();
	Tunables.TargetZSc2 = CVarCamTargetZ.GetValueOnGameThread();
	Tunables.NearSc2 = CVarCamNear.GetValueOnGameThread();
	Tunables.FarSc2 = CVarCamFar.GetValueOnGameThread();
	Tunables.ZoomStepSc2 = CVarCamZoomStep.GetValueOnGameThread();
	Tunables.ZoomReturnSpeed = CVarCamZoomReturn.GetValueOnGameThread();
	Tunables.UnitScaleUU = CVarCamUnitScale.GetValueOnGameThread();
	if (Tunables.UnitScaleUU <= 0.f)
	{
		Tunables.UnitScaleUU = KodCameraSettings::UnitScaleUU;
	}
	if (Tunables.DistanceMinSc2 > Tunables.DistanceSc2)
	{
		Tunables.DistanceMinSc2 = Tunables.DistanceSc2;
	}
	return Tunables;
}

FString FKodCameraTunables::DescribeScale() const
{
	const float Scale = UnitScaleUU;
	const float DepotCm = Scale * static_cast<float>(KodCameraSettings::DepotFootprintCells);
	return FString::Printf(
		TEXT("UnitScale=%.0f uu/SC2 cell=%d depot2x2=%.0fcm Distance=%.0fsc2->%.0fuu Min=%.0fsc2->%.0fuu Near=%.2fsc2->%.1fuu Far=%.0fsc2->%.0fuu TargetZ=%.2fsc2->%.1fuu"),
		Scale,
		KodBuildGrid::CellUU,
		DepotCm,
		DistanceSc2,
		KodCameraSettings::ToUU(DistanceSc2, Scale),
		DistanceMinSc2,
		KodCameraSettings::ToUU(DistanceMinSc2, Scale),
		NearSc2,
		KodCameraSettings::ToUU(NearSc2, Scale),
		FarSc2,
		KodCameraSettings::ToUU(FarSc2, Scale),
		TargetZSc2,
		KodCameraSettings::ToUU(TargetZSc2, Scale));
}
