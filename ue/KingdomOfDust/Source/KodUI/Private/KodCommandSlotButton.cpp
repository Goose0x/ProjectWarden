#include "KodCommandSlotButton.h"
#include "CommonTextBlock.h"
#include "Style/KodUIStyle.h"

void UKodCommandSlotButton::SetSlotIdentity(FName InCommandId, const FText& InGlyph, const FText& InAccessibleOrderName)
{
	CommandId = InCommandId;
	Glyph = InGlyph;
	AccessibleOrderName = InAccessibleOrderName;
	ApplyPresentation();
}

void UKodCommandSlotButton::SetChargeCount(int32 InChargeCount)
{
	ChargeCount = InChargeCount;
	ApplyPresentation();
}

void UKodCommandSlotButton::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	OnClicked().AddUObject(this, &UKodCommandSlotButton::HandleInternalClicked);
}

void UKodCommandSlotButton::HandleInternalClicked()
{
	OnSlotClicked.Broadcast(this);
}

void UKodCommandSlotButton::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetColorAndOpacity(UKodUIStyleLibrary::GetIronstockAmber());
	ApplyPresentation();
}

void UKodCommandSlotButton::ApplyPresentation()
{
	if (GlyphText)
	{
		GlyphText->SetText(Glyph);
		GlyphText->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetWhiteText()));
	}
	if (ChargeText)
	{
		if (ChargeCount >= 0)
		{
			ChargeText->SetVisibility(ESlateVisibility::HitTestInvisible);
			ChargeText->SetText(FText::AsNumber(ChargeCount));
		}
		else
		{
			ChargeText->SetVisibility(ESlateVisibility::Collapsed);
			ChargeText->SetText(FText::GetEmpty());
		}
	}
	SetToolTipText(AccessibleOrderName);
}
