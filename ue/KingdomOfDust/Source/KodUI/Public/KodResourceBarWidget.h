#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "KodResourceBarWidget.generated.h"

class UCommonTextBlock;
class UImage;

/**
 * Top-right resource strip. Ironstock. Icon + number only.
 * Chips are Cash, Oil, and Supply. No Credits. No crystal icons.
 * Designer preview 1250 / 680 / 48/60 is art. Gameplay supply cap is KodSupplyHardCap (200).
 * Parent WBP: /Game/UI/HUD/WBP_ResourceBar.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodResourceBarWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetCash(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetOil(int32 Amount);

	/** Clamps Max to KodSupplyHardCap and Current to that max. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetSupply(int32 Current, int32 Max);

	/** Icon and number tint from the active accent. Never cyan. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void ApplyHudAccent();

	UFUNCTION(BlueprintPure, Category = "Kod|UI")
	int32 GetDisplayedCash() const { return DisplayedCash; }

protected:
	virtual void NativePreConstruct() override;
	void ApplyValues();

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 DisplayedCash = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 DisplayedOil = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 DisplayedSupply = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI")
	int32 DisplayedSupplyMax = 0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UImage> Icon_Cash;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UCommonTextBlock> Value_Cash;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UImage> Icon_Oil;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UCommonTextBlock> Value_Oil;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UImage> Icon_Supply;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Resources")
	TObjectPtr<UCommonTextBlock> Value_Supply;
};
