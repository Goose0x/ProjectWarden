#pragma once

#include "CoreMinimal.h"
#include "KodOnlineService.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "KodOnlineSubsystem.generated.h"

/** Game-instance owner for IKodOnlineService. Defaults to the Null subsystem. */
UCLASS()
class KODNET_API UKodOnlineSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	IKodOnlineService& GetService();

private:
	TUniquePtr<IKodOnlineService> Service;
};
