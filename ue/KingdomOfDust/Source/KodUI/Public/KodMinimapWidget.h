#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "KodLabeledButton.h"
#include "KodMinimapWidget.generated.h"

class UCommonTextBlock;
class UImage;
class USizeBox;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKodMapToolRequested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodMinimapPingRequested, FVector, WorldLocation);

/**
 * Minimap cluster. Ironstock. Template plate v4.
 * MinimapImage is flush and owns the left bay. Idle worker, army, and ping are tiny chips
 * in ToolChipColumn on the RIGHT of that image. A header strip above the map is retired.
 * ClockText is a thin tab on the frame, not a row that steals bay height.
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

	void ApplyChipLayout();
	void ApplyToolChip(USizeBox* Box) const;
	void ApplyClockText();

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 ClockHours = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 ClockMinutes = 0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Minimap")
	TObjectPtr<UCommonTextBlock> ClockText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Minimap")
	TObjectPtr<USizeBox> Size_Clock;

	/** Vertical chip column on the right of MinimapImage. Not a header strip. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Minimap")
	TObjectPtr<UVerticalBox> ToolChipColumn;

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
