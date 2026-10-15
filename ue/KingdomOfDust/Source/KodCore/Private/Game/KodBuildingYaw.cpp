#include "Game/KodBuildingYaw.h"

#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	TAutoConsoleVariable<float> CVarKodBuildingYaw(
		TEXT("kod.BuildingYaw"),
		45.f,
		TEXT("Degrees added to every building mesh. The square grid footprint does not turn. 0 restores the old facing."),
		ECVF_Default);

	bool bIniApplied = false;

	void ApplyIniOnce()
	{
		if (bIniApplied)
		{
			return;
		}
		bIniApplied = true;
		IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(TEXT("kod.BuildingYaw"));
		if (!Var)
		{
			return;
		}
		const uint32 Priority = static_cast<uint32>(Var->GetFlags() & ECVF_SetByMask);
		if (Priority >= static_cast<uint32>(ECVF_SetByCode))
		{
			return;
		}
		float IniYaw = Var->GetFloat();
		if (GConfig->GetFloat(TEXT("KodMeshes"), TEXT("BuildingYawOffset"), IniYaw, GGameIni))
		{
			Var->Set(IniYaw, ECVF_SetByProjectSetting);
		}
	}
}

float KodBuildingYaw::OffsetDegrees()
{
	ApplyIniOnce();
	return CVarKodBuildingYaw.GetValueOnGameThread();
}

float KodBuildingYaw::ContainScale(float YawDegrees)
{
	const float Radians = FMath::DegreesToRadians(YawDegrees);
	const float Span = FMath::Abs(FMath::Cos(Radians)) + FMath::Abs(FMath::Sin(Radians));
	if (Span <= KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}
	return 1.f / Span;
}

float KodBuildingYaw::PresentationYaw(float RowYaw)
{
	return RowYaw + OffsetDegrees();
}
