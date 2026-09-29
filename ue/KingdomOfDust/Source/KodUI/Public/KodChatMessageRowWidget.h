#pragma once

#include "CommonUserWidget.h"
#include "KodChatMessageRowWidget.generated.h"

class UCommonTextBlock;

/**
 * One Versus chat line (MessageRow). Clan/name is cyan. Body is white.
 * Parent WBP: /Game/UI/Lobby/WBP_ChatMessageRow (W_ChatMessageRow).
 * Cyan glass only — no Ironstock.
 */
UCLASS(Blueprintable)
class KODUI_API UKodChatMessageRowWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void SetMessage(const FText& InSpeaker, const FText& InBody);

protected:
	virtual void NativePreConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> SpeakerText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> BodyText;

	FText Speaker;
	FText Body;
};
