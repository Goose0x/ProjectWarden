#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Sim/KodBuildTypes.h"
#include "KodBuildingSpawnBridge.generated.h"

/** Spawns building actors for placements recorded during the sim step. */
UCLASS()
class KODUNITS_API UKodBuildingSpawnBridge : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }
};
