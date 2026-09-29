#pragma once

#include "CommonButtonBase.h"
#include "KodCommandSlotButton.generated.h"

class UCommonTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodCommandSlotClicked, UKodCommandSlotButton*, Slot);

/**
 * One command-card cell. Face shows the hotkey glyph only.
 * Order names (Move, Stop, …) are tooltips, not button chrome.
 * Charge numerals are optional and must not compete with the glyph.
 * Parent WBP: /Game/UI/HUD/WBP_CommandSlot — name text blocks "GlyphText" and "ChargeText".
 */
UCLASS(Blueprintable)
class KODUI_API UKodCommandSlotButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Command")
	void SetSlotIdentity(FName InCommandId, const FText& InGlyph, const FText& InAccessibleOrderName);

	/** Pass a negative count to hide the charge numeral. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Command")
	void SetChargeCount(int32 InChargeCount);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Command")
	FName GetCommandId() const { return CommandId; }

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Command")
	FKodCommandSlotClicked OnSlotClicked;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	void HandleInternalClicked();

	void ApplyPresentation();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Command")
	TObjectPtr<UCommonTextBlock> GlyphText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Command")
	TObjectPtr<UCommonTextBlock> ChargeText;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Command")
	FName CommandId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Command")
	FText Glyph;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Command")
	FText AccessibleOrderName;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Command")
	int32 ChargeCount = INDEX_NONE;
};
