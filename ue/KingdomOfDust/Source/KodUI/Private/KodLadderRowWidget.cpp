#include "KodLadderRowWidget.h"
#include "CommonTextBlock.h"
#include "Style/KodUIStyle.h"

void UKodLadderRowWidget::ApplyRow(const FKodLadderRow& Row)
{
	const FSlateColor Color = Row.bLocalPlayer
		? FSlateColor(UKodUIStyleLibrary::GetOrangeCta())
		: FSlateColor(UKodUIStyleLibrary::GetWhiteText());

	if (RankText)
	{
		RankText->SetText(FText::AsNumber(Row.Rank));
		RankText->SetColorAndOpacity(Color);
	}
	if (NameText)
	{
		NameText->SetText(Row.Name);
		NameText->SetColorAndOpacity(Color);
	}
	if (MmrText)
	{
		MmrText->SetText(FText::AsNumber(Row.Mmr));
		MmrText->SetColorAndOpacity(Color);
	}
	if (WinsText)
	{
		WinsText->SetText(FText::AsNumber(Row.Wins));
		WinsText->SetColorAndOpacity(Color);
	}
	if (LossesText)
	{
		LossesText->SetText(FText::AsNumber(Row.Losses));
		LossesText->SetColorAndOpacity(Color);
	}
}
