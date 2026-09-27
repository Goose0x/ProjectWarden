#pragma once

#include "CommonUserWidget.h"
#include "KodPortraitFrameWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UProgressBar;

/**
 * Portrait column. Ironstock.
 * HP bar, then the ENERGY slot under it. ENERGY hides with Hidden (not Collapsed)
 * so the column does not reflow when a unit has no energy.
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

protected:
	virtual void NativePreConstruct() override;
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
};
