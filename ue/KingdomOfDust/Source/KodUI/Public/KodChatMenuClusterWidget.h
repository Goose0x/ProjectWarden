#pragma once

#include "CommonUserWidget.h"
#include "KodChatMenuClusterWidget.generated.h"

class UKodLabeledButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKodChatMenuClicked);

/**
 * Chat + Menu cluster sitting above the command card. Ironstock.
 * Help is optional: layout stamp v4 reserves it; paint v3 may omit the widget.
 * Parent WBP: /Game/UI/HUD/WBP_ChatMenuCluster.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodChatMenuClusterWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|HUD")
	FKodChatMenuClicked OnChatClicked;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|HUD")
	FKodChatMenuClicked OnMenuClicked;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|HUD")
	FKodChatMenuClicked OnHelpClicked;

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|HUD")
	void ApplyHudAccent();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	UFUNCTION()
	void HandleChat(UKodLabeledButton* Button);

	UFUNCTION()
	void HandleMenu(UKodLabeledButton* Button);

	UFUNCTION()
	void HandleHelp(UKodLabeledButton* Button);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UKodLabeledButton> Button_Chat;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|HUD")
	TObjectPtr<UKodLabeledButton> Button_Menu;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|HUD")
	TObjectPtr<UKodLabeledButton> Button_Help;
};
