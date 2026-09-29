#include "KodResourceBarWidget.h"
#include "KodResourceWallet.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Style/KodUILayout.h"
#include "Style/KodUIStyle.h"

void UKodResourceBarWidget::SetCash(int32 Amount)
{
	DisplayedCash = FMath::Max(0, Amount);
	ApplyValues();
}

void UKodResourceBarWidget::SetOil(int32 Amount)
{
	DisplayedOil = FMath::Max(0, Amount);
	ApplyValues();
}

void UKodResourceBarWidget::SetSupply(int32 Current, int32 Max)
{
	const int32 Cap = FMath::Clamp(Max, 0, KodSupplyHardCap);
	DisplayedSupplyMax = Cap;
	DisplayedSupply = FMath::Clamp(Current, 0, Cap);
	ApplyValues();
}

void UKodResourceBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	ApplyHudAccent();

	if (IsDesignTime())
	{
		SetCash(KodUILayout::StampPreviewCash);
		SetOil(KodUILayout::StampPreviewOil);
		SetSupply(KodUILayout::StampPreviewSupply, KodUILayout::StampPreviewSupplyMax);
	}
	ApplyValues();
}

void UKodResourceBarWidget::ApplyHudAccent()
{
	const FLinearColor Accent = UKodUIStyleLibrary::GetActiveHudAccentColor();
	if (Icon_Cash) { Icon_Cash->SetColorAndOpacity(Accent); }
	if (Icon_Oil) { Icon_Oil->SetColorAndOpacity(Accent); }
	if (Icon_Supply) { Icon_Supply->SetColorAndOpacity(Accent); }
	ApplyValues();
}

void UKodResourceBarWidget::ApplyValues()
{
	const FSlateColor Accent(UKodUIStyleLibrary::GetActiveHudAccentColor());
	if (Value_Cash)
	{
		Value_Cash->SetText(FText::AsNumber(DisplayedCash));
		Value_Cash->SetColorAndOpacity(Accent);
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
