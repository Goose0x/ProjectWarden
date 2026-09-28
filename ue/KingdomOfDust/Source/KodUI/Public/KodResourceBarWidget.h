#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "KodResourceBarWidget.generated.h"

class UCommonTextBlock;
class UImage;

/**
 * Top-right resource strip. Ironstock. Icon + number only.
 * Chips are Credits, Oil, and Supply — no word chrome, no crystal icons.
 * M1 Dust Crystal feeds the Credits chip through SetDustDisplay.
 * Parent WBP: /Game/UI/HUD/WBP_ResourceBar.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodResourceBarWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	/** Back-compat. Writes the Credits chip and DisplayedDust. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetDustDisplay(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetCredits(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetOil(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetSupply(int32 Current, int32 Max);

	/** Ironstock readout tint. USA amber or RSF rust. Never cyan. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void ApplyHudAccent();

	UFUNCTION(BlueprintPure, Category = "Kod|UI")
	int32 GetDisplayedCredits() const { return DisplayedCredits; }

protected:
	virtual void NativePreConstruct() override;
	void ApplyValues();

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 DisplayedDust = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 DisplayedCredits = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 DisplayedOil = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 DisplayedSupply = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 DisplayedSupplyMax = 0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UImage> Icon_Credits;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UCommonTextBlock> Value_Credits;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UImage> Icon_Oil;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UCommonTextBlock> Value_Oil;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UImage> Icon_Supply;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UCommonTextBlock> Value_Supply;
};
