// Kingdom of Dust — primary game module
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FKingdomOfDustModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
