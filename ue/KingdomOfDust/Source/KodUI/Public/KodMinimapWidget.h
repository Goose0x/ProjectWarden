#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "KodLabeledButton.h"
#include "KodMinimapWidget.generated.h"

class UCommonTextBlock;
class UImage;
class USizeBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKodMapToolRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodMinimapPingRequested, FVector, WorldLocation);

/**
 * Minimap cluster. Ironstock.
 * Clock sits above/left. Map tools are idle worker, army, and ping — a vertical stack beside the square map.
 * Parent WBP: /Game/UI/HUD/WBP_Minimap.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodMinimapWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void PingWorldLocation(FVector WorldLocation);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetMatchClock(int32 Hours, int32 Minutes);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void NotifyIdleWorkers();

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void NotifyArmy();

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void ApplyHudAccent();

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI")
	FKodMapToolRequested OnIdleWorkersRequested;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI")
	FKodMapToolRequested OnArmyRequested;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI")
	FKodMinimapPingRequested OnPingRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	UFUNCTION()
	void HandleIdleClicked(UKodLabeledButton* Button);

	UFUNCTION()
	void HandleArmyClicked(UKodLabeledButton* Button);

	UFUNCTION()
	void HandlePingClicked(UKodLabeledButton* Button);

	void ApplyToolFloor(USizeBox* Box) const;
	void ApplyClockText();

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 ClockHours = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 ClockMinutes = 0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Minimap")
	TObjectPtr<UCommonTextBlock> ClockText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Minimap")
	TObjectPtr<USizeBox> Size_Clock;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Minimap")
	TObjectPtr<UKodLabeledButton> Tool_IdleWorker;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Minimap")
	TObjectPtr<UKodLabeledButton> Tool_Army;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Minimap")
	TObjectPtr<UKodLabeledButton> Tool_Ping;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Minimap")
	TObjectPtr<USizeBox> Size_IdleWorker;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Minimap")
	TObjectPtr<USizeBox> Size_Army;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Minimap")
	TObjectPtr<USizeBox> Size_Ping;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Minimap")
	TObjectPtr<UImage> MinimapImage;
};
