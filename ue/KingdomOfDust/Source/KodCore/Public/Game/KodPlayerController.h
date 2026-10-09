#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Sim/KodEntityId.h"
#include "KodPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UMaterialInterface;
class AActor;

/** Saved mesh custom-depth state so a selection highlight can be cleared. */
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
 * never an actor pivot. A sim-registered combat actor on that ray, other than the
 * current selection, is Attack. Resource nodes are selectable. RMB with a Dozer
 * on a node starts gather. Marines still Move to the impact point. RMB ground
 * cancels gather. A node is never an attack target, and neither is a drop-off.
 * Sim actors on another TeamId are not added to the local selection (click, drag,
 * or rim) but stay valid Attack targets.
 *
 * LMB drag past BoxSelectDragThresholdPx is a screen-space marquee. On release,
 * every local-team sim actor whose viewport projection lies in the rect is
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

	/** Drop one actor and refresh the rim when it was selected. Does not write sim state. */
	void RemoveFromLocalSelection(AActor* Actor);

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
	/** RMB: Attack a foreign sim entity, Gather when a worker hits a node, otherwise Move. */
	void HandleRightClickCommand();

	struct FKodCursorCommand
	{
		AActor* AttackActor = nullptr;
		FKodEntityId AttackId;
		AActor* GatherActor = nullptr;
		FKodEntityId GatherId;
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
	/** AKodPlayerState::TeamId, or 0 when the player state is not a Kod player. */
	int32 GetLocalTeamId() const;

	void ApplySelectionHighlight();
	void ClearSelectionHighlight();
	/** Soft-load M_SelectionRim once and cache it. Null after a failed load. */
	UMaterialInterface* GetSelectionOverlayMaterial();

	/** Override / BlueprintImplementable for UKodCommandSubsystem enqueue. Default: sim IssueMove. */
	UFUNCTION(BlueprintNativeEvent, Category = "Kod|Command")
	void IssueMoveToSelection(FVector WorldLocation);
	virtual void IssueMoveToSelection_Implementation(FVector WorldLocation);

	UFUNCTION(BlueprintNativeEvent, Category = "Kod|Command")
	void IssueAttackToSelection(AActor* Target);
	virtual void IssueAttackToSelection_Implementation(AActor* Target);

	/**
	 * Workers on the selection gather Node. Everyone else Moves to NonGathererMovePoint.
	 * Slice 0 enqueues both through UKodCommandSubsystem.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Kod|Command")
	void IssueGatherToSelection(AActor* Node, FVector NonGathererMovePoint);
	virtual void IssueGatherToSelection_Implementation(AActor* Node, FVector NonGathererMovePoint);

	void BeginMarquee(FVector2D ScreenPos);
	void UpdateMarquee(FVector2D ScreenPos);
	void EndMarquee(bool bAddToSelection);
	void ClickSelectAtCursor(bool bAddToSelection);
	/**
	 * First sim-registered actor along the cursor ray.
	 * ECC_Pawn is walked past blockers with no sim id (greybox PROXY_* / ground).
	 * Visibility is used only when the pawn channel hits nothing.
	 * Foreign-team sim actors are walked past (attackable, not locally selectable).
	 * Resource nodes are selectable (they are not a team and are not attackable).
	 * Unregistered-only rays return null so the click clears selection.
	 * OutHitLog lists each blocker as Name(Channel,sim,id,loc) for PIE.
	 */
	AActor* TraceSelectableUnderCursor(FString& OutHitLog) const;
	/** Local-team sim actors (not the camera pawn) whose screen point lies in the marquee. */
	void CollectActorsInMarquee(TArray<AActor*>& OutActors) const;
	void GetMarqueeBounds(FVector2D& OutMin, FVector2D& OutMax) const;
	/** Viewport-relative projection, matching GetMousePosition and the marquee corners. */
	bool ProjectActorToMarqueeSpace(const AActor* Actor, FVector2D& OutScreen) const;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> LocalSelection;

	UPROPERTY(Transient)
	TArray<FKodSelectionHighlightState> SelectionHighlights;

	/** Cached rim overlay. Null when the soft path failed to load. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> SelectionOverlayMaterial;

	bool bSelectionOverlayResolved = false;

	bool bSelectPressed = false;
	bool bMarqueeActive = false;
	FVector2D MarqueeStart = FVector2D::ZeroVector;
	FVector2D MarqueeEnd = FVector2D::ZeroVector;
};
