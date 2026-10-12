#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "KodBuildGhost.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/** Cursor preview for a building footprint. Not a sim entity. */
UCLASS()
class KODUNITS_API AKodBuildGhost : public AActor
{
	GENERATED_BODY()

public:
	AKodBuildGhost();

	void SetFootprint(int32 CellsX, int32 CellsY);
	void SetValidTint(bool bValid);

private:
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Tint;
};
