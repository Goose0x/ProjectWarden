#include "KodPortraitFrameWidget.h"
#include "KodUI.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Style/KodUILayout.h"
#include "Style/KodUIStyle.h"

namespace KodPortraitPrivate
{
	void LockBarHeight(UProgressBar* Bar, float Height)
	{
		if (!Bar)
		{
			return;
		}
		FProgressBarStyle Style = Bar->GetWidgetStyle();
		Style.BackgroundImage.ImageSize.Y = Height;
		Style.FillImage.ImageSize.Y = Height;
		Style.MarqueeImage.ImageSize.Y = Height;
		Bar->SetWidgetStyle(Style);
	}

	void KeepEnergySlot(UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}
		if (Widget->GetVisibility() == ESlateVisibility::Collapsed)
		{
			Widget->SetVisibility(ESlateVisibility::Hidden);
			UE_LOG(LogKodUI, Warning, TEXT("%s was Collapsed. ENERGY stays Hidden so the slot is not eaten."), *Widget->GetName());
		}
		if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Widget->Slot))
		{
			FSlateChildSize Size(ESlateSizeRule::Automatic);
			BoxSlot->SetSize(Size);
			BoxSlot->SetPadding(FMargin(0.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		if (USizeBox* Box = Cast<USizeBox>(Widget->GetParent()))
		{
			Box->SetMinDesiredHeight(KodUILayout::PortraitEnergyBarHeightPx);
		}
	}

	bool SharesCanvas(const UWidget* A, const UWidget* B)
	{
		return A && B && A->GetParent() && A->GetParent() == B->GetParent()
			&& Cast<UCanvasPanelSlot>(A->Slot) && Cast<UCanvasPanelSlot>(B->Slot);
	}
}

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
	ApplyPortraitGeometry();
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
	if (IsDesignTime())
	{
		HitPoints = static_cast<float>(KodUILayout::StampPreviewHp);
		HitPointsMax = static_cast<float>(KodUILayout::StampPreviewHpMax);
		bHasEnergy = true;
		Energy = 1.f;
		EnergyMax = 1.f;
	}
	ApplyPortraitGeometry();
	ApplyBars();
}

void UKodPortraitFrameWidget::ApplyPortraitGeometry()
{
	using namespace KodPortraitPrivate;

	const bool bEnergyOnFrame = SharesCanvas(PortraitImage, EnergyBar);
	const float CaptionHeight = (bEnergyOnFrame && EnergyCaption) ? KodUILayout::PortraitEnergyCaptionHeightPx : 0.f;
	const float EnergyHeight = bEnergyOnFrame ? KodUILayout::PortraitEnergyBarHeightPx : 0.f;
	const float EnergyBlock = CaptionHeight + EnergyHeight;

	if (PortraitImage)
	{
		FSlateBrush Brush = PortraitImage->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Margin = FMargin(0.f);
		Brush.Tiling = ESlateBrushTileType::NoTile;
		PortraitImage->SetBrush(Brush);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PortraitImage->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			CanvasSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, EnergyBlock));
			CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetZOrder(0);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(PortraitImage->Slot))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Fill);
			OverlaySlot->SetPadding(FMargin(0.f));
		}
		else if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(PortraitImage->Slot))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = 1.f;
			BoxSlot->SetSize(Size);
			BoxSlot->SetPadding(FMargin(0.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
			BoxSlot->SetVerticalAlignment(VAlign_Fill);
		}
		else
		{
			UE_LOG(LogKodUI, Warning, TEXT("Parent PortraitImage to a canvas or overlay so the portrait fills the frame edge-to-edge."));
		}
	}

	if (HPBar)
	{
		LockBarHeight(HPBar, KodUILayout::PortraitHpBarHeightPx);
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HPBar->Slot))
		{
			// Bottom of the bar meets the bottom of the portrait frame. No gap.
			CanvasSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
			CanvasSlot->SetAlignment(FVector2D(0.f, 1.f));
			CanvasSlot->SetOffsets(FMargin(0.f, -EnergyBlock, 0.f, KodUILayout::PortraitHpBarHeightPx));
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetZOrder(1);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(HPBar->Slot))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
			OverlaySlot->SetPadding(FMargin(0.f));
		}
		else if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(HPBar->Slot))
		{
			FSlateChildSize Size(ESlateSizeRule::Automatic);
			BoxSlot->SetSize(Size);
			BoxSlot->SetPadding(FMargin(0.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		else
		{
			UE_LOG(LogKodUI, Warning, TEXT("Parent HPBar to the portrait canvas so it sits flush on the bottom of the frame."));
		}
	}

	if (HPValue)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(HPValue->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
			CanvasSlot->SetAlignment(FVector2D(1.f, 1.f));
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetOffsets(FMargin(-KodUILayout::PortraitHpValueInsetPx, -EnergyBlock, 0.f, 0.f));
			CanvasSlot->SetZOrder(2);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(HPValue->Slot))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Right);
			OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
			OverlaySlot->SetPadding(FMargin(0.f, 0.f, KodUILayout::PortraitHpValueInsetPx, 0.f));
		}
	}

	if (bEnergyOnFrame)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(EnergyBar->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
			CanvasSlot->SetAlignment(FVector2D(0.f, 1.f));
			CanvasSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, KodUILayout::PortraitEnergyBarHeightPx));
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetZOrder(1);
		}
		if (EnergyCaption)
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(EnergyCaption->Slot))
			{
				CanvasSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
				CanvasSlot->SetAlignment(FVector2D(0.f, 1.f));
				CanvasSlot->SetAutoSize(false);
				CanvasSlot->SetOffsets(FMargin(0.f, -KodUILayout::PortraitEnergyBarHeightPx, 0.f, CaptionHeight));
				CanvasSlot->SetZOrder(1);
			}
		}
	}
	else if (EnergyBar && PortraitImage && EnergyBar->GetParent() == PortraitImage->GetParent())
	{
		UE_LOG(LogKodUI, Warning, TEXT("Place PortraitImage, HPBar, and EnergyBar on one canvas so ENERGY stays under the frame without covering the portrait."));
	}

	KeepEnergySlot(EnergyBar);
	KeepEnergySlot(EnergyCaption);
	LockBarHeight(EnergyBar, KodUILayout::PortraitEnergyBarHeightPx);

	if (HPBar && EnergyBar)
	{
		UPanelWidget* Parent = HPBar->GetParent();
		if (Parent && Parent == EnergyBar->GetParent() && !Cast<UCanvasPanelSlot>(HPBar->Slot))
		{
			if (Parent->GetChildIndex(EnergyBar) < Parent->GetChildIndex(HPBar))
			{
				UE_LOG(LogKodUI, Warning, TEXT("ENERGY must sit under portrait HP. Keep the slot; hide it with Hidden, not Collapsed."));
			}
		}
	}
}

void UKodPortraitFrameWidget::ApplyBars()
{
	const float HpFraction = HitPointsMax > 0.f ? HitPoints / HitPointsMax : 0.f;
	const float EnergyFraction = EnergyMax > 0.f ? Energy / EnergyMax : 0.f;
	const FLinearColor Readout = UKodUIStyleLibrary::GetIronstockAmber();

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
		EnergyCaption->SetColorAndOpacity(FSlateColor(Readout));
		EnergyCaption->SetVisibility(EnergyVisibility);
	}
	if (EnergyBar)
	{
		EnergyBar->SetPercent(FMath::Clamp(EnergyFraction, 0.f, 1.f));
		EnergyBar->SetFillColorAndOpacity(Readout);
		// Hidden keeps the reserved slot. Collapsed would give that space away for good.
		EnergyBar->SetVisibility(EnergyVisibility);
	}
	if (PortraitImage)
	{
		PortraitImage->SetColorAndOpacity(UKodUIStyleLibrary::GetIronstockMetal());
	}
}
