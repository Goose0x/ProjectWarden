#pragma once

#include "KodUIScreenRoot.h"
#include "Style/KodUITypes.h"
#include "KodMainMenuWidget.generated.h"

class UCommonTextBlock;
class UImage;
class UKodIdentityStripWidget;
class UKodLabeledButton;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodMainMenuActionSignature, EKodMainMenuAction, Action);

/**
 * Front-door root. Cyan glass. Not Ironstock.
 * Single orange commit: PLAY VERSUS. No faction tiles, ladder, or PLAY RANKED.
 * Parent WBP: /Game/UI/MainMenu/WBP_MainMenu.
 *
 * TRAINING is the deep-link hook toward the Versus TRAINING sub-tab.
 * This widget does not navigate by itself.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodMainMenuWidget : public UKodUIScreenRoot
{
	GENERATED_BODY()

public:
	UKodMainMenuWidget();

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|MainMenu")
	FKodMainMenuActionSignature OnMainMenuAction;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	void ApplyStampCopy();
	void TintButton(UKodLabeledButton* Button, bool bCommit) const;

	UFUNCTION()
	void HandlePlayVersus(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleTraining(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleCampaign(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleCoop(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleArmory(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleOptions(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleExit(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleFriends(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleHelp(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleMenu(UKodLabeledButton* Button);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UImage> HeroArt;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|MainMenu")
	TObjectPtr<UImage> GlassPlate;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodIdentityStripWidget> Identity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodLabeledButton> Button_PlayVersus;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UCommonTextBlock> Caption_VersusLobby;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodLabeledButton> Nav_Training;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodLabeledButton> Nav_Campaign;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodLabeledButton> Nav_Coop;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodLabeledButton> Nav_Armory;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodLabeledButton> Nav_Options;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodLabeledButton> Nav_Exit;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodLabeledButton> Util_Friends;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodLabeledButton> Util_Help;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UKodLabeledButton> Util_Menu;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UWidget> BriefingPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UCommonTextBlock> BriefingTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UCommonTextBlock> Briefing_Patch;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UCommonTextBlock> Briefing_FeaturedMap;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UCommonTextBlock> Briefing_Season;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UCommonTextBlock> Footer_Version;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|MainMenu")
	TObjectPtr<UCommonTextBlock> Footer_Online;
};
