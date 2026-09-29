// KodCore — shared game framework, sim, FOW, tags
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FKodCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
