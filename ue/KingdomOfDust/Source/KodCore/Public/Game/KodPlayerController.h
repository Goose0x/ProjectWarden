#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Sim/KodEntityId.h"
#include "KodPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class AActor;

/**
 * RTS player controller — Slice 0 walk gate hooks.
 *
 * Enhanced Input (bind in SetupInputComponent / BP):
 *   IMC_KodRTS · IA_Select · IA_CommandMove · IA_CommandAttack · camera axes
 * Selection + marquee live here; orders should flow through UKodCommandSubsystem
 * (see AKodSlice0PlayerController in KingdomOfDust module for full enqueue wiring).
 */
UCLASS(Blueprintable)
class KODCORE_API AKodPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AKodPlayerController();

	virtual void BeginPlay() override;
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

	/** Ground hit under cursor (channel Visibility / camera). */
	UFUNCTION(BlueprintCallable, Category = "Kod|Input")
	bool GetGroundHitUnderCursor(FHitResult& OutHit) const;

protected:
	void HandleSelectStarted();
	void HandleSelectCompleted();
	void HandleCommandMove();
	void HandleCommandAttack();

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
	void CollectActorsInMarquee(TArray<AActor*>& OutActors) const;

	UPROPERTY()
	TArray<TWeakObjectPtr<AActor>> LocalSelection;

	bool bSelectPressed = false;
	bool bMarqueeActive = false;
	FVector2D MarqueeStart = FVector2D::ZeroVector;
	FVector2D MarqueeEnd = FVector2D::ZeroVector;
};
