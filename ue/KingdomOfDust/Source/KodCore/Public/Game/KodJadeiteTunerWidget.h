#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "KodJadeiteTunerWidget.generated.h"

class UImage;
class USlider;
class UTextBlock;

/**
 * Dev panel for the Jadeite palette. Built in C++. F9 toggles it.
 * F10 is already the pause menu. Campaign and offline skirmish only.
 * Sliders write the material parameter collection immediately, so PIE
 * updates without a material rebuild. Nothing here touches the sim.
 */
UCLASS()
class KODCORE_API UKodJadeiteTunerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	bool IsCursorOverPanel() const;

	UFUNCTION()
	void HandleHueChanged(float Value);

	UFUNCTION()
	void HandleSaturationChanged(float Value);

	UFUNCTION()
	void HandleBrightnessChanged(float Value);

	UFUNCTION()
	void HandleEmissiveChanged(float Value);

	UFUNCTION()
	void HandleSliderCaptureEnd();

	UFUNCTION()
	void HandleReset();

	UFUNCTION()
	void HandleCopyHex();

private:
	void BuildTree();
	void PullFromPalette();
	void RefreshReadout();
	class UKodJadeitePaletteSubsystem* Palette() const;

	UPROPERTY()
	TObjectPtr<USlider> HueSlider;

	UPROPERTY()
	TObjectPtr<USlider> SaturationSlider;

	UPROPERTY()
	TObjectPtr<USlider> BrightnessSlider;

	UPROPERTY()
	TObjectPtr<USlider> EmissiveSlider;

	UPROPERTY()
	TObjectPtr<UTextBlock> HueLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> SaturationLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> BrightnessLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> EmissiveLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> HexText;

	UPROPERTY()
	TObjectPtr<UImage> ShadowSwatch;

	UPROPERTY()
	TObjectPtr<UImage> BaseSwatch;

	UPROPERTY()
	TObjectPtr<UImage> LitSwatch;

	UPROPERTY()
	TObjectPtr<UImage> RimSwatch;

	UPROPERTY()
	TObjectPtr<UImage> CoreSwatch;

	bool bMuteSlider = false;
};
