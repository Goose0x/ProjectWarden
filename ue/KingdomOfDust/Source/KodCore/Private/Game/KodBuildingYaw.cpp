#include "Game/KodBuildingYaw.h"

#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	TAutoConsoleVariable<float> CVarKodBuildingYaw(
		TEXT("kod.BuildingYaw"),
		0.f,
		TEXT("Debug yaw added to every building mesh. Default 0; art is authored diagonal. The square grid footprint does not turn."),
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

float KodBuildingYaw::PresentationYaw(float RowYaw)
{
	return RowYaw + OffsetDegrees();
}
