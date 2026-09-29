#pragma once

#include "CommonUserWidget.h"
#include "Style/KodUITypes.h"
#include "KodLobbyChatRailWidget.generated.h"

class UBorder;
class UCommonTextBlock;
class UEditableText;
class UImage;
class UKodChatMessageRowWidget;
class UKodLabeledButton;
class UScrollBox;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodLobbyChatSubmitted, FText, Message);

/**
 * Versus chat component v1. Cyan glass. RIGHT rail only.
 * Quiet next to orange PLAY RANKED. No Ironstock.
 *
 * W_ChatRail (ChatRoot)
 *   ChannelTabs — Tab_Lobby, Tab_Party
 *   MessageList — MessageRow children
 *   ChatInput — Text_Message, Btn_Send
 *
 * Parent WBP: /Game/UI/Lobby/WBP_LobbyChatRail.
 * The Versus shell parents this widget as ChatRail, the last LobbyColumns child.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodLobbyChatRailWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Lobby")
	static EKodLobbyChatRail GetRequiredRail();

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void SetActiveTab(EKodLobbyChatTab Tab);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void AppendMessage(const FText& Speaker, const FText& Body);

	/** Single-line entry. Text before the first ": " is the speaker. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void AppendLine(const FText& Line);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void SubmitInput();

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodLobbyChatSubmitted OnChatSubmitted;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI|Lobby")
	TSubclassOf<UKodChatMessageRowWidget> MessageRowClass;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;

	void ApplyTabChrome();
	void ApplyGlassChrome();
	void RebuildMessages();
	void SeedStampPreview();

	UFUNCTION()
	void HandleLobbyTab(UKodLabeledButton* Button);

	UFUNCTION()
	void HandlePartyTab(UKodLabeledButton* Button);

	UFUNCTION()
	void HandleSend(UKodLabeledButton* Button);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Lobby")
	EKodLobbyChatTab ActiveTab = EKodLobbyChatTab::Lobby;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UBorder> ChatRoot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UWidget> ChannelTabs;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Tab_Lobby;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Tab_Party;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Lobby")
	TObjectPtr<UImage> Underline_Lobby;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Lobby")
	TObjectPtr<UImage> Underline_Party;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UScrollBox> MessageList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UWidget> ChatInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UEditableText> Text_Message;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Btn_Send;

	TArray<FKodChatLine> Lines;
	bool bPreviewSeeded = false;
};
