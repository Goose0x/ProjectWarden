#pragma once

#include "CommonUserWidget.h"
#include "KodPortraitFrameWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UProgressBar;
class USizeBox;

/**
 * W_PortraitPanel. Geometry is the v1.1 pin. Faction palette retint is not applied here.
 *
 * Art box (overlay or canvas):
 *   Img_Portrait fills the art edge-to-edge. No letterbox.
 *   Prog_Health is integrated on the bottom of that art box. Not a bay under the frame.
 *   Txt_Callsign is the only callsign text. Copy is "Black Widow".
 * Slot_Energy is directly under the art box at a fixed height.
 *   Prog_Energy hides when unused. The slot height stays.
 *
 * Parent WBP: /Game/UI/HUD/WBP_PortraitFrame.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodPortraitFrameWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Portrait")
	void SetVitals(float HitPoints, float HitPointsMax, float Energy, float EnergyMax, bool bHasEnergy);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Portrait")
	void SetEnergyVisible(bool bVisible);

	/** Reapplies v1.1 geometry. Does not retint a faction palette. HP stays green. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Portrait")
	void ApplyHudAccent();

	/** The only face-card callsign. Two words. */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Portrait")
	static FText GetSilkCallsign();

protected:
	virtual void NativePreConstruct() override;
	void ApplyPortraitGeometry();
	void ApplyBars();

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Portrait")
	float HitPoints = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Portrait")
	float HitPointsMax = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Portrait")
	float Energy = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Portrait")
	float EnergyMax = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Portrait")
	bool bHasEnergy = false;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Portrait")
	TObjectPtr<UImage> Img_Portrait;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Portrait")
	TObjectPtr<UProgressBar> Prog_Health;

	/** Optional numeral on the health bar (75/75). Not a callsign. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Portrait")
	TObjectPtr<UCommonTextBlock> Txt_Health;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Portrait")
	TObjectPtr<USizeBox> Slot_Energy;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Portrait")
	TObjectPtr<UProgressBar> Prog_Energy;

	/** The only callsign widget. Do not add a second Black Widow label. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Portrait")
	TObjectPtr<UCommonTextBlock> Txt_Callsign;
};
