#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Sim/KodEntityId.h"
#include "KodPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UMaterialInstanceDynamic;
class AActor;

/** Saved mesh look so selection tint / custom depth can be cleared. */
USTRUCT()
struct KODCORE_API FKodSelectionHighlightState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UMeshComponent> Mesh;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> OriginalMaterials;

	UPROPERTY()
	bool bHadCustomDepth = false;

	UPROPERTY()
	int32 OriginalStencil = 0;
};

/**
 * RTS player controller — Slice 0 walk gate hooks.
 *
 * Enhanced Input (bind in SetupInputComponent / BP):
 *   IMC_KodRTS · IA_Select · IA_CommandMove · IA_CommandAttack · camera axes
 * Selection + marquee live here; orders should flow through UKodCommandSubsystem
 * (see AKodSlice0PlayerController in KingdomOfDust module for full enqueue wiring).
 *
 * RMB walks ECC_Pawn (camera pawn ignored), same as click-select. The first
 * blocking ImpactPoint is the Move point for ground and other non-sim props —
 * never an actor pivot. A sim-registered actor on that ray, other than the
 * current selection, is Attack.
 *
 * LMB drag past BoxSelectDragThresholdPx is a screen-space marquee. On release,
 * every sim-registered actor whose viewport projection lies in the rect is
 * selected. Left Shift adds; a fresh drag replaces. A shorter drag stays
 * click-select. AKodHUD paints the rect with the engine canvas (no Content asset).
 */
UCLASS(Blueprintable)
class KODCORE_API AKodPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKodPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Input")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Input")
	TSoftObjectPtr<UInputAction> SelectAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Kod|Input")
	TSoftObjectPtr<UInputAction> CommandMoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|Selection")
	float BoxSelectDragThresholdPx = 6.f;

	UFUNCTION(BlueprintCallable, Category = "Kod|Camera")
	void PanCamera(FVector2D AxisValue);

	UFUNCTION(BlueprintCallable, Category = "Kod|Camera")
	void ZoomCamera(float AxisValue);

	/** Resolved selection. Stale weak refs are omitted; LocalSelection stays weak. */
	UFUNCTION(BlueprintPure, Category = "Kod|Selection")
	TArray<AActor*> GetLocalSelection() const;

	UFUNCTION(BlueprintCallable, Category = "Kod|Selection")
	TArray<FKodEntityId> GetLocalSelectedEntityIds() const;

	/**
	 * Drag rect in GetMousePosition pixels. False until the pressed drag
	 * reaches BoxSelectDragThresholdPx. AKodHUD reads this to paint the box.
	 */
	bool GetSelectionMarqueeRect(FVector2D& OutMin, FVector2D& OutMax) const;

	/** Ground hit under cursor (channel Visibility / camera). */
	UFUNCTION(BlueprintCallable, Category = "Kod|Input")
	bool GetGroundHitUnderCursor(FHitResult& OutHit) const;

protected:
	void HandleSelectStarted();
	void HandleSelectCompleted();
	void HandleCommandMove();
	void HandleCommandAttack();
	/** RMB: Attack a foreign sim entity on the ray, otherwise Move to the first ImpactPoint. */
	void HandleRightClickCommand();

	struct FKodCursorCommand
	{
		AActor* AttackActor = nullptr;
		FKodEntityId AttackId;
		bool bHasMovePoint = false;
		bool bSim = false;
		FVector MovePoint = FVector::ZeroVector;
		FString HitName;
	};

	/**
	 * Cursor ray on ECC_Pawn. MovePoint is the first blocking ImpactPoint.
	 * AttackActor is the first sim entity on that ray that is not the current selection.
	 * Non-sim props are walked past for Attack and never contribute their pivot.
	 * Visibility is used only when the pawn channel hits nothing.
	 */
	FKodCursorCommand TraceCursorCommand() const;
	bool IsInLocalSelection(const AActor* Actor) const;

	void ApplySelectionHighlight();
	void ClearSelectionHighlight();
	UMaterialInstanceDynamic* GetSelectionTintMaterial();

	/** Override / BlueprintImplementable for UKodCommandSubsystem enqueue. Default: sim IssueMove. */
	UFUNCTION(BlueprintNativeEvent, Category = "Kod|Command")
	void IssueMoveToSelection(FVector WorldLocation);
	virtual void IssueMoveToSelection_Implementation(FVector WorldLocation);

	UFUNCTION(BlueprintNativeEvent, Category = "Kod|Command")
	void IssueAttackToSelection(AActor* Target);
	virtual void IssueAttackToSelection_Implementation(AActor* Target);

	void BeginMarquee(FVector2D ScreenPos);
	void UpdateMarquee(FVector2D ScreenPos);
	void EndMarquee(bool bAddToSelection);
	void ClickSelectAtCursor(bool bAddToSelection);
	/**
	 * First sim-registered actor along the cursor ray.
	 * ECC_Pawn is walked past blockers with no sim id (greybox PROXY_* / ground).
	 * Visibility is used only when the pawn channel hits nothing.
	 * Unregistered-only rays return null so the click clears selection.
	 * OutHitLog lists each blocker as Name(Channel,sim,id,loc) for PIE.
	 */
	AActor* TraceSelectableUnderCursor(FString& OutHitLog) const;
	/** Sim-registered actors (not the camera pawn) whose screen point lies in the marquee. */
	void CollectActorsInMarquee(TArray<AActor*>& OutActors) const;
	void GetMarqueeBounds(FVector2D& OutMin, FVector2D& OutMax) const;
	/** Viewport-relative projection, matching GetMousePosition and the marquee corners. */
	bool ProjectActorToMarqueeSpace(const AActor* Actor, FVector2D& OutScreen) const;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> LocalSelection;

	UPROPERTY(Transient)
	TArray<FKodSelectionHighlightState> SelectionHighlights;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SelectionTintMid;

	bool bSelectPressed = false;
	bool bMarqueeActive = false;
	FVector2D MarqueeStart = FVector2D::ZeroVector;
	FVector2D MarqueeEnd = FVector2D::ZeroVector;
};
