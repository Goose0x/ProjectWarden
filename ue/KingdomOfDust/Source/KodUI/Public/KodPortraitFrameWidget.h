#pragma once

#include "CommonUserWidget.h"
#include "KodPortraitFrameWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UProgressBar;

/**
 * W_PortraitPanel. Ironstock paint v3.
 * PortraitImage fills the frame edge-to-edge. HPBar is flush to the bottom of that frame.
 * ENERGY sits under HP. Hide it with Hidden, never Collapsed, so the slot stays reserved.
 * Parent WBP: /Game/UI/HUD/WBP_PortraitFrame. Root should be a canvas.
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

	/** Reapplies the portrait pin. Does not paint an accent re-skin. HP stays green. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Portrait")
	void ApplyHudAccent();

	/** Face-card callsign when a portrait shows one. Two words. */
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
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Portrait")
	TObjectPtr<UProgressBar> HPBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Portrait")
	TObjectPtr<UCommonTextBlock> HPValue;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Portrait")
	TObjectPtr<UProgressBar> EnergyBar;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Portrait")
	TObjectPtr<UCommonTextBlock> EnergyCaption;

	/** Optional. Reads "Black Widow" when a face card shows a callsign. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Portrait")
	TObjectPtr<UCommonTextBlock> CallsignText;
};
