#include "KodIdentityStripWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"

UKodIdentityStripWidget::UKodIdentityStripWidget()
{
	CommanderName = NSLOCTEXT("KodUI", "PreviewCommander", "Commander Doe");
	ClanTag = NSLOCTEXT("KodUI", "PreviewClan", "WARD");
}

void UKodIdentityStripWidget::SetIdentity(const FText& InCommanderName, const FText& InClanTag)
{
	CommanderName = InCommanderName;
	ClanTag = InClanTag;
	ApplyIdentity();
}

void UKodIdentityStripWidget::SetLeagueLine(const FText& InLeagueLine)
{
	LeagueLine = InLeagueLine;
	ApplyIdentity();
}

FText UKodIdentityStripWidget::FormatClanTag(const FText& Tag)
{
	const FString Raw = Tag.ToString().TrimStartAndEnd();
	if (Raw.IsEmpty())
	{
		return NSLOCTEXT("KodUI", "DefaultClan", "[WARD]");
	}
	if (Raw.StartsWith(TEXT("[")) && Raw.EndsWith(TEXT("]")))
	{
		return FText::FromString(Raw);
	}
	return FText::FromString(FString::Printf(TEXT("[%s]"), *Raw));
}

void UKodIdentityStripWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyIdentity();
}

void UKodIdentityStripWidget::ApplyIdentity()
{
	if (NameText)
	{
		NameText->SetText(CommanderName);
	}
	if (ClanTagText)
	{
		ClanTagText->SetText(FormatClanTag(ClanTag));
	}
	if (LeagueLineText)
	{
		const bool bHasLeague = !LeagueLine.IsEmpty();
		LeagueLineText->SetVisibility(bHasLeague ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		LeagueLineText->SetText(LeagueLine);
	}
}
