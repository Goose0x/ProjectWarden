#pragma once

#include "CommonButtonBase.h"
#include "KodLabeledButton.generated.h"

class UCommonTextBlock;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodLabeledButtonClicked, UKodLabeledButton*, Button);

/**
 * Reused hit target for stamped text buttons and icon-only utilities.
 * Parent a Widget Blueprint at /Game/UI/CommonUI/WBP_LabeledButton.
 * Name the optional text block "Label" and the optional image "Icon".
 */
UCLASS(Blueprintable)
class KODUI_API UKodLabeledButton : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetStampLabel(const FText& InLabel);

	/** Icon-only buttons (lobby Friends / Help / Menu) hide the word and keep the tooltip. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void SetShowLabel(bool bInShowLabel);

	UFUNCTION(BlueprintPure, Category = "Kod|UI")
	FText GetStampLabel() const { return LabelText; }

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI")
	FKodLabeledButtonClicked OnLabeledClicked;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	void HandleInternalClicked();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI")
	TObjectPtr<UCommonTextBlock> Label;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI")
	TObjectPtr<UImage> Icon;

	FText LabelText;
	bool bShowLabel = true;
};
