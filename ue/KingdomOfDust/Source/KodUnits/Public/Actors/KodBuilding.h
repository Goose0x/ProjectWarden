#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "Sim/KodEntityId.h"
#include "KodBuilding.generated.h"

class UKodBuildingDefinition;
class UKodBuildQueueComponent;
class UKodSimSubsystem;
class USkeletalMeshComponent;
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
	virtual void Tick(float DeltaSeconds) override;

	/** 0, 1, 2, 3 for build progress under 33%, 66%, 100%, and finished. Art reads this. */
	int32 GetScaffoldStage() const;

	/** Smoke under 35% HP and fire under 15%. Empty Niagara slots do nothing. */
	void UpdateLowHealthFx(float Health, float MaxHealth);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Kod|Data")
	TSoftObjectPtr<UKodBuildingDefinition> Definition;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|Sim")
	FKodEntityId EntityId;

	UPROPERTY(BlueprintReadWrite, Category = "Kod|Team")
	int32 TeamId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Mesh")
	TObjectPtr<UStaticMeshComponent> BuildingMesh;

	/** Set when the catalog row is a skeletal mesh. No AnimBP. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Kod|Mesh")
	TObjectPtr<USkeletalMeshComponent> BuildingSkel;

	UFUNCTION(BlueprintCallable, Category = "Kod|Data")
	UKodBuildingDefinition* GetDefinition() const;

	UFUNCTION(BlueprintCallable, Category = "Kod|Data")
	void ApplyDefinition(UKodBuildingDefinition* Def);

private:
	bool bBoundStructure = false;
	FVector BaseScale = FVector(3.f, 3.f, 2.f);
	float MeshYaw = 0.f;
	float MeshZ = 0.f;
	bool bSawIncomplete = false;
	bool bSawComplete = false;
	bool bWasAirborne = false;
	bool bCompletionFlash = false;
	float FlashLeft = 0.f;
	bool bLoggedLowHealth = false;
	/** 0 raised, 1 flush. Presentation only; the sim flag is bLowered. */
	float DepotLowerAlpha = 0.f;
	bool bDepotVisualInit = false;
	bool bDepotLowerKnown = false;
	bool bDepotLowerLatched = false;
	bool bDepotClipDriven = false;
	void PlayDepotCommandAnim(bool bLower);
	void SyncBunkerTurret(UKodSimSubsystem* Sim, float DeltaSeconds);
	void FlashBunkerMuzzle();
	bool bLoggedTurretSocket = false;
	bool bLoggedPortSocket = false;
	int32 SeenMuzzle = 0;
	int32 NextPort = 0;
	float MuzzleFlashLeft = 0.f;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> TurretPieces;

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> BunkerMuzzle;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> BunkerMuzzleLight;

	UPROPERTY()
	TObjectPtr<UKodBuildingDefinition> CachedDef;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> SmokeFx;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> FireFx;

	void PreloadLowHealthFx();
	void BindJadeitePalette();
	void UnbindJadeitePalette();
	void RefreshJadeiteEmissive();

	FDelegateHandle JadeitePaletteHandle;
	FStreamableManager LowHealthStream;
	TSharedPtr<FStreamableHandle> LowHealthHandle;
};
