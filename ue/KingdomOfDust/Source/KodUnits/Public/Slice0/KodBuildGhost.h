#pragma once

#include "CoreMinimal.h"
#include "Game/KodLanding.h"
#include "GameFramework/Actor.h"
#include "KodBuildGhost.generated.h"

class UStaticMeshComponent;
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
class USceneComponent;

/** Cursor preview for a building footprint. Not a sim entity. */
UCLASS()
class KODUNITS_API AKodBuildGhost : public AActor
{
	GENERATED_BODY()

public:
	AKodBuildGhost();

	void SetFootprint(int32 CellsX, int32 CellsY);
	void SetCells(int32 CellsX, int32 CellsY, const TArray<uint8>& CellOk);
	void SetValidTint(bool bValid);
	/** Building mesh at the shared yaw. The cell pad stays on the square grid. */
	void SetPreview(FName TypeId);
	/** One preview for placement and landing. Transform is the canonical pose. */
	void Show(FName TypeId, const FKodLandingPose& Pose, const TArray<uint8>& CellOk, bool bValid);

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> PreviewSkel;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> CellMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CellTints;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> Tint;
};
