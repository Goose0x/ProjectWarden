#include "KodPortraitFrameWidget.h"
#include "KodUI.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Style/KodUILayout.h"
#include "Style/KodUIStyle.h"

void UKodPortraitFrameWidget::SetVitals(float InHitPoints, float InHitPointsMax, float InEnergy, float InEnergyMax, bool bInHasEnergy)
{
	HitPoints = InHitPoints;
	HitPointsMax = InHitPointsMax;
	Energy = InEnergy;
	EnergyMax = InEnergyMax;
	bHasEnergy = bInHasEnergy;
	ApplyBars();
}

void UKodPortraitFrameWidget::SetEnergyVisible(bool bVisible)
{
	bHasEnergy = bVisible;
	ApplyBars();
}

void UKodPortraitFrameWidget::ApplyHudAccent()
{
	ApplyBars();
}

FText UKodPortraitFrameWidget::GetSilkCallsign()
{
	return NSLOCTEXT("KodUI", "SilkCallsign", "Black Widow");
}

void UKodPortraitFrameWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (EnergyCaption)
	{
		EnergyCaption->SetText(NSLOCTEXT("KodUI", "EnergyCaption", "ENERGY"));
	}
	if (CallsignText)
	{
		CallsignText->SetText(GetSilkCallsign());
	}
	if (HPBar && EnergyBar)
	{
		UPanelWidget* Parent = HPBar->GetParent();
		if (Parent && Parent == EnergyBar->GetParent() && Parent->GetChildIndex(EnergyBar) < Parent->GetChildIndex(HPBar))
		{
			UE_LOG(LogKodUI, Warning, TEXT("ENERGY must sit under portrait HP. Keep the slot; hide it with Hidden, not Collapsed."));
		}
	}
	if (IsDesignTime())
	{
		HitPoints = static_cast<float>(KodUILayout::StampPreviewHp);
		HitPointsMax = static_cast<float>(KodUILayout::StampPreviewHpMax);
		bHasEnergy = true;
		Energy = 1.f;
		EnergyMax = 1.f;
	}
	ApplyBars();
}

void UKodPortraitFrameWidget::ApplyBars()
{
	const float HpFraction = HitPointsMax > 0.f ? HitPoints / HitPointsMax : 0.f;
	const float EnergyFraction = EnergyMax > 0.f ? Energy / EnergyMax : 0.f;

	if (HPBar)
	{
		HPBar->SetPercent(FMath::Clamp(HpFraction, 0.f, 1.f));
		HPBar->SetFillColorAndOpacity(UKodUIStyleLibrary::GetHpGreen());
	}
	if (HPValue)
	{
		const int32 Current = FMath::RoundToInt(HitPoints);
		const int32 Max = FMath::RoundToInt(HitPointsMax);
		HPValue->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), Current, Max)));
		HPValue->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetWhiteText()));
	}
	const ESlateVisibility EnergyVisibility = bHasEnergy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
	if (EnergyCaption)
	{
		EnergyCaption->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetActiveHudAccentColor()));
		EnergyCaption->SetVisibility(EnergyVisibility);
	}
	if (EnergyBar)
	{
		EnergyBar->SetPercent(FMath::Clamp(EnergyFraction, 0.f, 1.f));
		EnergyBar->SetFillColorAndOpacity(UKodUIStyleLibrary::GetActiveHudAccentColor());
		// Hidden keeps layout space. Collapsed would reflow the portrait column.
		EnergyBar->SetVisibility(EnergyVisibility);
	}
	if (PortraitImage)
	{
		PortraitImage->SetColorAndOpacity(UKodUIStyleLibrary::GetIronstockMetal());
	}
}
