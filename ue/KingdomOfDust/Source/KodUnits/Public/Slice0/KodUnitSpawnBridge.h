#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Sim/KodProductionTypes.h"
#include "KodUnitSpawnBridge.generated.h"

/**
 * Spawns actors for sim train completions. The sim records the request during
 * the step; this drains it afterwards so RegisterEntity is not inside the step.
 */
UCLASS()
class KODUNITS_API UKodUnitSpawnBridge : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return true; }
	virtual bool IsTickableInEditor() const override { return false; }

private:
	void SpawnOne(const FKodPendingUnitSpawn& Request);
};
