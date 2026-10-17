#pragma once

#include "CoreMinimal.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"
#include "Sim/KodEntityId.h"
#include "Engine/StaticMesh.h"
#include "Sim/KodDepotClips.h"
#include "KodBuilding.generated.h"

class UAnimSequence;
class UMeshComponent;
class USkeletalMesh;
struct FKodMeshRow;

class UKodBuildingDefinition;
class UKodBuildQueueComponent;
class UKodSimSubsystem;
class USkeletalMeshComponent;
class UStaticMeshComponent;
struct FKodStructureState;

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

	/** 0, 1, 2 while building, 3 when finished. Thresholds are kod.Build.Stage2 and Stage3. */
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
	bool bDepotClipDriven = false;
	bool bDepotSkelTried = false;
	bool bDepotPlateApplied = false;
	bool bDepotCollisionInit = false;
	bool bDepotPawnOpen = false;
	uint8 DepotFxPose = 255;
	int32 DepotFxTicks = -1;
	int32 AmberPhasesLeft = 0;
	float AmberPhaseLeft = 0.f;
	bool bAmberLit = false;
	FName ActiveDepotClip;
	bool bDepotSequenceLoop = false;

	UPROPERTY()
	TObjectPtr<UAnimSequence> DepotSequence;
	FName ActiveMeshId;
	int32 ShownStage = -1;
	bool bDeferredAttach = false;
	bool bFinalPrimaryBody = false;
	bool bConstructionMeshSwap = false;
	bool bConstructionHidden = false;
	bool bFinalSkeletal = false;
	bool bFinalStaticHidden = false;
	bool bFinalSkelHidden = true;
	bool bLoggedStageFallback = false;
	bool bLoggedDynastyGhost = false;
	bool bPoolResolved = false;
	bool bPoolMissing = false;
	int32 PoolVisualStage = -1;
	float PoolZFrom = 0.f;
	float PoolZTo = 0.f;
	float PoolZElapsed = 0.f;

	UPROPERTY()
	TObjectPtr<UStaticMesh> FinalStaticMesh;

	/** Post-demo dynasty ghost. Created only when kod.Build.DynastyGhost is on. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> DynastyGhost;

	/** RiteWell pool. Visual only. Absent when the ini assets do not load. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> PoolSurface;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> PoolMotes;

	UPROPERTY()
	TObjectPtr<UNiagaraComponent> PoolPulse;
	void SyncDepotPresentation(const FKodStructureState& Structure, float DeltaSeconds);
	void TryUseDepotSkeleton();
	void ApplyDepotPlateClip(UMeshComponent* Mesh);
	void ApplyDepotPawnCollision(bool bWalkable);
	void PlayDepotClip(const FKodMeshRow& Row, FName ClipId, bool bLoop);
	void SyncDepotClipPosition(const FKodStructureState& Structure);
	void PumpDepotNotifies(const FKodStructureState& Structure);
	void HandleDepotNotify(const FKodDepotNotify& Notify, int32 AtTick);
	void PulseDepotAmber();
	void TickDepotAmber(float DeltaSeconds);
	void SetDepotAmber(float Value);
	UAnimSequence* ResolveDepotSequence(const FKodMeshRow& Row, FName ClipId) const;
	USkeletalMesh* ResolveDepotSkeleton(const FKodMeshRow& Row) const;
	FVector DepotSocketLocation(FName SocketName) const;
	FString DepotClipToken(const FKodMeshRow& Row, FName ClipId) const;
	void ApplyConstructionVisual(UKodSimSubsystem* Sim, FKodStructureState& Structure, FVector& Scale);
	void RestoreFinalPresentation(UKodSimSubsystem* Sim);
	void UpdateDynastyGhost(float Progress);
	void UpdatePoolSurface(float DeltaSeconds, int32 Stage);
	void SetConstructionAttachesVisible(bool bVisible);
	void RememberFinalMesh();
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
