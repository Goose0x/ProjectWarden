#include "KodResourceBarWidget.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Style/KodUILayout.h"
#include "Style/KodUIStyle.h"

void UKodResourceBarWidget::SetDustDisplay(int32 Amount)
{
	DisplayedDust = Amount;
	SetCredits(Amount);
}

void UKodResourceBarWidget::SetCredits(int32 Amount)
{
	DisplayedCredits = Amount;
	DisplayedDust = Amount;
	ApplyValues();
}

void UKodResourceBarWidget::SetOil(int32 Amount)
{
	DisplayedOil = Amount;
	ApplyValues();
}

void UKodResourceBarWidget::SetSupply(int32 Current, int32 Max)
{
	DisplayedSupply = Current;
	DisplayedSupplyMax = Max;
	ApplyValues();
}

void UKodResourceBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyHudAccent();

	if (IsDesignTime())
	{
		DisplayedCredits = KodUILayout::StampPreviewCredits;
		DisplayedDust = KodUILayout::StampPreviewCredits;
		DisplayedOil = KodUILayout::StampPreviewOil;
		DisplayedSupply = KodUILayout::StampPreviewSupply;
		DisplayedSupplyMax = KodUILayout::StampPreviewSupplyMax;
	}
	ApplyValues();
}

void UKodResourceBarWidget::ApplyHudAccent()
{
	const FLinearColor Accent = UKodUIStyleLibrary::GetActiveHudAccentColor();
	if (Icon_Credits) { Icon_Credits->SetColorAndOpacity(Accent); }
	if (Icon_Oil) { Icon_Oil->SetColorAndOpacity(Accent); }
	if (Icon_Supply) { Icon_Supply->SetColorAndOpacity(Accent); }
	ApplyValues();
}

void UKodResourceBarWidget::ApplyValues()
{
	const FSlateColor Accent(UKodUIStyleLibrary::GetActiveHudAccentColor());
	if (Value_Credits)
	{
		Value_Credits->SetText(FText::AsNumber(DisplayedCredits));
		Value_Credits->SetColorAndOpacity(Accent);
	}
	if (Value_Oil)
	{
		Value_Oil->SetText(FText::AsNumber(DisplayedOil));
		Value_Oil->SetColorAndOpacity(Accent);
	}
	if (Value_Supply)
	{
		Value_Supply->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), DisplayedSupply, DisplayedSupplyMax)));
		Value_Supply->SetColorAndOpacity(Accent);
	}
}
