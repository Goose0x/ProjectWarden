#include "KodPortraitFrameWidget.h"
#include "KodUI.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
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

	void FillCanvas(UCanvasPanelSlot* CanvasSlot, float BottomMargin, int32 ZOrder)
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, BottomMargin));
		CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetZOrder(ZOrder);
	}

	void DockHealthToArtBottom(UCanvasPanelSlot* CanvasSlot, float ArtBottomInset)
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
		CanvasSlot->SetAlignment(FVector2D(0.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f, -ArtBottomInset, 0.f, KodUILayout::PortraitHpBarHeightPx));
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetZOrder(2);
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
	if (Txt_Callsign)
	{
		Txt_Callsign->SetText(GetSilkCallsign());
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

	const float SlotHeight = KodUILayout::PortraitEnergySlotHeightPx;
	const bool bSlotOnArtCanvas = SharesCanvas(Img_Portrait, Slot_Energy);
	const float ArtBottomInset = bSlotOnArtCanvas ? SlotHeight : 0.f;

	if (Img_Portrait)
	{
		FSlateBrush Brush = Img_Portrait->GetBrush();
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Brush.Margin = FMargin(0.f);
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Img_Portrait->SetBrush(Brush);

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Img_Portrait->Slot))
		{
			FillCanvas(CanvasSlot, ArtBottomInset, 0);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Img_Portrait->Slot))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Fill);
			OverlaySlot->SetPadding(FMargin(0.f));
		}
		else if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Img_Portrait->Slot))
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
			UE_LOG(LogKodUI, Warning, TEXT("Parent Img_Portrait to the art overlay or canvas so the face fills the box edge-to-edge."));
		}
	}

	if (Prog_Health)
	{
		LockBarHeight(Prog_Health, KodUILayout::PortraitHpBarHeightPx);
		const bool bSeparateBay = Img_Portrait
			&& Img_Portrait->GetParent() == Prog_Health->GetParent()
			&& Cast<UVerticalBoxSlot>(Img_Portrait->Slot)
			&& Cast<UVerticalBoxSlot>(Prog_Health->Slot);
		if (bSeparateBay)
		{
			UE_LOG(LogKodUI, Error, TEXT("Prog_Health is a row under Img_Portrait. Dock it to the bottom of the art box. Do not build a separate HP bay."));
		}

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Prog_Health->Slot))
		{
			DockHealthToArtBottom(CanvasSlot, ArtBottomInset);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Prog_Health->Slot))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
			OverlaySlot->SetPadding(FMargin(0.f));
		}
		else if (!bSeparateBay)
		{
			UE_LOG(LogKodUI, Warning, TEXT("Parent Prog_Health to the same art overlay or canvas as Img_Portrait, flush to that box's bottom edge."));
		}
	}

	if (Txt_Health)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Txt_Health->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f));
			CanvasSlot->SetAlignment(FVector2D(1.f, 1.f));
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetOffsets(FMargin(-KodUILayout::PortraitHpValueInsetPx, -ArtBottomInset, 0.f, 0.f));
			CanvasSlot->SetZOrder(3);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Txt_Health->Slot))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Right);
			OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
			OverlaySlot->SetPadding(FMargin(0.f, 0.f, KodUILayout::PortraitHpValueInsetPx, 0.f));
		}
	}

	if (Txt_Callsign)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Txt_Callsign->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
			CanvasSlot->SetAlignment(FVector2D(0.5f, 0.f));
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetOffsets(FMargin(0.f, KodUILayout::PortraitCallsignInsetPx, 0.f, 0.f));
			CanvasSlot->SetZOrder(4);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Txt_Callsign->Slot))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Center);
			OverlaySlot->SetVerticalAlignment(VAlign_Top);
			OverlaySlot->SetPadding(FMargin(0.f, KodUILayout::PortraitCallsignInsetPx, 0.f, 0.f));
		}
	}

	if (Slot_Energy)
	{
		Slot_Energy->SetHeightOverride(SlotHeight);
		Slot_Energy->SetMinDesiredHeight(SlotHeight);
		if (Slot_Energy->GetVisibility() == ESlateVisibility::Collapsed)
		{
			Slot_Energy->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			UE_LOG(LogKodUI, Warning, TEXT("Slot_Energy was Collapsed. The ENERGY slot keeps a fixed height."));
		}
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot_Energy->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
			CanvasSlot->SetAlignment(FVector2D(0.f, 1.f));
			CanvasSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, SlotHeight));
			CanvasSlot->SetAutoSize(false);
			CanvasSlot->SetZOrder(1);
		}
		else if (UVerticalBoxSlot* BoxSlot = Cast<UVerticalBoxSlot>(Slot_Energy->Slot))
		{
			FSlateChildSize Size(ESlateSizeRule::Automatic);
			BoxSlot->SetSize(Size);
			BoxSlot->SetPadding(FMargin(0.f));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(Slot_Energy->Slot))
		{
			UE_LOG(LogKodUI, Warning, TEXT("Slot_Energy is inside the art overlay. Place it under the art box, directly beneath HP."));
			OverlaySlot->SetPadding(FMargin(0.f));
		}
	}

	if (Prog_Energy)
	{
		LockBarHeight(Prog_Energy, KodUILayout::PortraitHpBarHeightPx);
		if (Slot_Energy && Prog_Energy->GetParent() != Slot_Energy)
		{
			UE_LOG(LogKodUI, Warning, TEXT("Parent Prog_Energy inside Slot_Energy. Hide the fill, not the slot."));
		}
		if (USizeBoxSlot* EnergySlot = Cast<USizeBoxSlot>(Prog_Energy->Slot))
		{
			EnergySlot->SetPadding(FMargin(0.f));
			EnergySlot->SetHorizontalAlignment(HAlign_Fill);
			EnergySlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (Prog_Energy->GetVisibility() == ESlateVisibility::Collapsed)
		{
			Prog_Energy->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UKodPortraitFrameWidget::ApplyBars()
{
	const float HpFraction = HitPointsMax > 0.f ? HitPoints / HitPointsMax : 0.f;
	const float EnergyFraction = EnergyMax > 0.f ? Energy / EnergyMax : 0.f;

	if (Prog_Health)
	{
		Prog_Health->SetPercent(FMath::Clamp(HpFraction, 0.f, 1.f));
		Prog_Health->SetFillColorAndOpacity(UKodUIStyleLibrary::GetHpGreen());
	}
	if (Txt_Health)
	{
		const int32 Current = FMath::RoundToInt(HitPoints);
		const int32 Max = FMath::RoundToInt(HitPointsMax);
		Txt_Health->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), Current, Max)));
		Txt_Health->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetWhiteText()));
	}
	if (Txt_Callsign)
	{
		Txt_Callsign->SetText(GetSilkCallsign());
		Txt_Callsign->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetWhiteText()));
	}
	if (Prog_Energy)
	{
		Prog_Energy->SetPercent(FMath::Clamp(EnergyFraction, 0.f, 1.f));
		Prog_Energy->SetFillColorAndOpacity(UKodUIStyleLibrary::GetActiveHudAccentColor());
		Prog_Energy->SetVisibility(bHasEnergy ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
	if (Img_Portrait)
	{
		Img_Portrait->SetColorAndOpacity(FLinearColor::White);
	}
}
