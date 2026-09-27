#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Sim/KodEntityId.h"
#include "KodBuilding.generated.h"

class UKodBuildingDefinition;
class UKodBuildQueueComponent;
class UStaticMeshComponent;

/**
 * Stationary production / power / defense structure.
 * Build queue lives on UKodBuildQueueComponent (KodEconomy).
 */
UCLASS(Blueprintable)
class KODUNITS_API AKodBuilding : public AActor
{
	GENERATED_BODY()

public:
	AKodBuilding();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kod|Data")
	TSoftObjectPtr<UKodBuildingDefinition> Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId EntityId;

	UPROPERTY(BlueprintReadWrite, Category = "Kod|Team")
	int32 TeamId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Mesh")
	TObjectPtr<UStaticMeshComponent> BuildingMesh;

	UFUNCTION(BlueprintCallable, Category = "Kod|Data")
	UKodBuildingDefinition* GetDefinition() const;

	UFUNCTION(BlueprintCallable, Category = "Kod|Data")
	void ApplyDefinition(UKodBuildingDefinition* Def);
};
