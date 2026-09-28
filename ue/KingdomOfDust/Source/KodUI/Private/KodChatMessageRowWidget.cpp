#include "KodChatMessageRowWidget.h"
#include "CommonTextBlock.h"
#include "Style/KodUIStyle.h"

void UKodChatMessageRowWidget::SetMessage(const FText& InSpeaker, const FText& InBody)
{
	Speaker = InSpeaker;
	Body = InBody;
	if (SpeakerText)
	{
		SpeakerText->SetText(Speaker);
		SpeakerText->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetCyanActive()));
	}
	if (BodyText)
	{
		BodyText->SetText(Body);
		BodyText->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetWhiteText()));
	}
}

void UKodChatMessageRowWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetMessage(Speaker, Body);
}
