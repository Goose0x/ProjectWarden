#include "KodLabeledButton.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"

void UKodLabeledButton::SetStampLabel(const FText& InLabel)
{
	LabelText = InLabel;
	SetToolTipText(InLabel);
	if (Label)
	{
		Label->SetText(InLabel);
		Label->SetVisibility(bShowLabel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UKodLabeledButton::SetShowLabel(bool bInShowLabel)
{
	bShowLabel = bInShowLabel;
	if (Label)
	{
		Label->SetVisibility(bShowLabel ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UKodLabeledButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	OnClicked().AddUObject(this, &UKodLabeledButton::HandleInternalClicked);
}

void UKodLabeledButton::HandleInternalClicked()
{
	OnLabeledClicked.Broadcast(this);
}

void UKodLabeledButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (!LabelText.IsEmpty())
	{
		SetStampLabel(LabelText);
	}
	else
	{
		SetShowLabel(bShowLabel);
	}
}
