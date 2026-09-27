#pragma once

#include "CommonUserWidget.h"
#include "Style/KodUITypes.h"
#include "KodLobbyChatRailWidget.generated.h"

class UCommonTextBlock;
class UEditableText;
class UKodLabeledButton;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodLobbyChatSubmitted, FText, Message);

/**
 * Versus lobby chat. Stamp v2.6 places this rail on the RIGHT.
 * GetRequiredRail() has no left-side result.
 * Tabs: LOBBY / PARTY. History, input, and send stay secondary to PLAY RANKED.
 * Parent WBP: /Game/UI/Lobby/WBP_LobbyChatRail.
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
	void AppendLine(const FText& Line);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void SubmitInput();

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodLobbyChatSubmitted OnChatSubmitted;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	void ApplyTabChrome();
	void ApplyHistoryText();

	UFUNCTION()
	void HandleLobbyTab(UKodLabeledButton* Button);

	UFUNCTION()
	void HandlePartyTab(UKodLabeledButton* Button);

	UFUNCTION()
	void HandleSend(UKodLabeledButton* Button);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Lobby")
	EKodLobbyChatTab ActiveTab = EKodLobbyChatTab::Lobby;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Tab_Lobby;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Tab_Party;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> ChatHistory;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UEditableText> ChatInput;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> ChatSend;

	TArray<FText> Lines;
};
