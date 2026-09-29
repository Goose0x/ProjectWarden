#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "KodBuildQueueComponent.generated.h"

class UKodUnitDefinition;

/**
 * One-slot build / train queue (M1).
 * Enqueue a unit definition; tick Progress until complete, then spawn.
 */
UCLASS(ClassGroup = (Kod), meta = (BlueprintSpawnableComponent))
class KODECONOMY_API UKodBuildQueueComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UKodBuildQueueComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Soft ref currently in the single queue slot (null if idle). */
	UPROPERTY(BlueprintReadOnly, Category = "Kod|Build")
	TSoftObjectPtr<UKodUnitDefinition> QueuedUnit;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Build")
	float ProgressSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Build")
	float RequiredSeconds = 0.f;

	UFUNCTION(BlueprintPure, Category = "Kod|Build")
	bool IsBusy() const { return !QueuedUnit.IsNull(); }

	UFUNCTION(BlueprintPure, Category = "Kod|Build")
	float GetProgressAlpha() const
	{
		return RequiredSeconds > 0.f ? FMath::Clamp(ProgressSeconds / RequiredSeconds, 0.f, 1.f) : 0.f;
	}

	/** Returns false if already busy or Def invalid. Does not spend resources — caller does. */
	UFUNCTION(BlueprintCallable, Category = "Kod|Build")
	bool Enqueue(UKodUnitDefinition* Def);

	UFUNCTION(BlueprintCallable, Category = "Kod|Build")
	void Cancel();

	/** Spawn transform for completed unit (defaults to owner forward offset). */
	UFUNCTION(BlueprintCallable, Category = "Kod|Build")
	FTransform GetSpawnTransform() const;

protected:
	void CompleteCurrent();

	UPROPERTY(EditAnywhere, Category = "Kod|Build")
	float SpawnForwardOffset = 300.f;
};
