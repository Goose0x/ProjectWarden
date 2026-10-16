#include "Game/KodBuildVisual.h"

#include "HAL/IConsoleManager.h"
#include "Warden/KodWardenPaths.h"

namespace
{
	TAutoConsoleVariable<float> CVarStage2(
		TEXT("kod.Build.Stage2"),
		0.33f,
		TEXT("Build progress that swaps a slice building to stage 2."),
		ECVF_Default);

	TAutoConsoleVariable<float> CVarStage3(
		TEXT("kod.Build.Stage3"),
		0.66f,
		TEXT("Build progress that swaps a slice building to stage 3."),
		ECVF_Default);

	TAutoConsoleVariable<int32> CVarDynastyGhost(
		TEXT("kod.Build.DynastyGhost"),
		0,
		TEXT("0 keeps the dynasty ghost off. Post-demo hook for a fading copy of the final mesh. Disabled for the Oct 20 demo."),
		ECVF_Default);
}

bool KodBuildVisual::IsSliceBuilding(FName Id)
{
	return Id == FName(KodWardenPaths::Id_CommandCenter)
		|| Id == FName(KodWardenPaths::Id_Barracks)
		|| Id == FName(KodWardenPaths::Id_SupplyDepot)
		|| Id == FName(KodWardenPaths::Id_Rig)
		|| Id == FName(KodWardenPaths::Id_EngineeringBay)
		|| Id == FName(KodWardenPaths::Id_Bunker)
		|| Id == FName(KodWardenPaths::Id_Turret)
		|| Id == FName(TEXT("ScinderHQ"))
		|| Id == FName(TEXT("RiteWell"))
		|| Id == FName(TEXT("ScinderHost"));
}

EKodBuildVisual KodBuildVisual::ParseMode(const FString& Text)
{
	if (Text.Equals(TEXT("Stages"), ESearchCase::IgnoreCase))
	{
		return EKodBuildVisual::Stages;
	}
	if (Text.Equals(TEXT("Vfx"), ESearchCase::IgnoreCase))
	{
		return EKodBuildVisual::Vfx;
	}
	return EKodBuildVisual::Scaffold;
}

EKodBuildVisual KodBuildVisual::Resolve(FName BuildingId, EKodBuildVisual Requested, bool bHasStageMeshes)
{
	if (Requested == EKodBuildVisual::Vfx)
	{
		return EKodBuildVisual::Vfx;
	}
	if (Requested == EKodBuildVisual::Stages && IsSliceBuilding(BuildingId) && bHasStageMeshes)
	{
		return EKodBuildVisual::Stages;
	}
	return EKodBuildVisual::Scaffold;
}

float KodBuildVisual::Progress(int32 TicksDone, int32 TicksTotal)
{
	if (TicksTotal <= 0)
	{
		return 1.f;
	}
	return FMath::Clamp(static_cast<float>(TicksDone) / static_cast<float>(TicksTotal), 0.f, 1.f);
}

int32 KodBuildVisual::StageIndex(float ProgressValue)
{
	const float Clamped = FMath::Clamp(ProgressValue, 0.f, 1.f);
	if (Clamped >= 0.999f)
	{
		return 3;
	}
	const float Third = CVarStage3.GetValueOnAnyThread();
	const float Second = CVarStage2.GetValueOnAnyThread();
	if (Clamped >= Third)
	{
		return 2;
	}
	if (Clamped >= Second)
	{
		return 1;
	}
	return 0;
}

bool KodBuildVisual::DynastyGhostEnabled()
{
	return CVarDynastyGhost.GetValueOnAnyThread() != 0;
}
