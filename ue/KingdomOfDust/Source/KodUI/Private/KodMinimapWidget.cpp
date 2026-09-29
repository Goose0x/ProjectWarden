#include "KodMinimapWidget.h"
#include "KodUI.h"
#include "KodLabeledButton.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Style/KodUILayout.h"
#include "Style/KodUIStyle.h"

void UKodMinimapWidget::PingWorldLocation(FVector WorldLocation)
{
	OnPingRequested.Broadcast(WorldLocation);
}

void UKodMinimapWidget::SetMatchClock(int32 Hours, int32 Minutes)
{
	ClockHours = Hours;
	ClockMinutes = Minutes;
	ApplyClockText();
}

void UKodMinimapWidget::NotifyIdleWorkers()
{
	OnIdleWorkersRequested.Broadcast();
}

void UKodMinimapWidget::NotifyArmy()
{
	OnArmyRequested.Broadcast();
}

void UKodMinimapWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Tool_IdleWorker)
	{
		Tool_IdleWorker->SetShowLabel(false);
		Tool_IdleWorker->SetStampLabel(NSLOCTEXT("KodUI", "MapToolIdle", "Idle workers"));
		Tool_IdleWorker->OnLabeledClicked.AddUniqueDynamic(this, &UKodMinimapWidget::HandleIdleClicked);
	}
	if (Tool_Army)
	{
		Tool_Army->SetShowLabel(false);
		Tool_Army->SetStampLabel(NSLOCTEXT("KodUI", "MapToolArmy", "Army"));
		Tool_Army->OnLabeledClicked.AddUniqueDynamic(this, &UKodMinimapWidget::HandleArmyClicked);
	}
	if (Tool_Ping)
	{
		Tool_Ping->SetShowLabel(false);
		Tool_Ping->SetStampLabel(NSLOCTEXT("KodUI", "MapToolPing", "Ping"));
		Tool_Ping->OnLabeledClicked.AddUniqueDynamic(this, &UKodMinimapWidget::HandlePingClicked);
	}
}

void UKodMinimapWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (IsDesignTime() && ClockHours == 0 && ClockMinutes == 0)
	{
		ClockHours = KodUILayout::StampPreviewClockHours;
		ClockMinutes = KodUILayout::StampPreviewClockMinutes;
	}

	ApplyHudAccent();

	if (Size_Clock)
	{
		Size_Clock->SetMinDesiredWidth(KodUILayout::ClockMinWidthPx);
	}
	ApplyChipLayout();
	ApplyClockText();
}

void UKodMinimapWidget::ApplyHudAccent()
{
	const FKodHudAccentColors Colors = UKodUIStyleLibrary::GetActiveHudAccentColors();
	const FLinearColor Accent = Colors.Proud;
	if (MinimapImage) { MinimapImage->SetColorAndOpacity(Colors.Metal); }
	if (Tool_IdleWorker) { Tool_IdleWorker->SetColorAndOpacity(Accent); }
	if (Tool_Army) { Tool_Army->SetColorAndOpacity(Accent); }
	if (Tool_Ping) { Tool_Ping->SetColorAndOpacity(Accent); }
	ApplyClockText();
}

void UKodMinimapWidget::HandleIdleClicked(UKodLabeledButton* Button)
{
	(void)Button;
	NotifyIdleWorkers();
}

void UKodMinimapWidget::HandleArmyClicked(UKodLabeledButton* Button)
{
	(void)Button;
	NotifyArmy();
}

void UKodMinimapWidget::HandlePingClicked(UKodLabeledButton* Button)
{
	(void)Button;
	PingWorldLocation(FVector::ZeroVector);
}

void UKodMinimapWidget::ApplyToolChip(USizeBox* Box) const
{
	if (!Box)
	{
		return;
	}
	const float Chip = KodUILayout::MapToolChipSizePx;
	Box->SetWidthOverride(Chip);
	Box->SetHeightOverride(Chip);
	Box->SetMinDesiredWidth(Chip);
	Box->SetMinDesiredHeight(Chip);
}

void UKodMinimapWidget::ApplyChipLayout()
{
	ApplyToolChip(Size_IdleWorker);
	ApplyToolChip(Size_Army);
	ApplyToolChip(Size_Ping);

	if (ToolChipColumn)
	{
		if (UHorizontalBoxSlot* ColumnSlot = Cast<UHorizontalBoxSlot>(ToolChipColumn->Slot))
		{
			FSlateChildSize Auto(ESlateSizeRule::Automatic);
			ColumnSlot->SetSize(Auto);
			ColumnSlot->SetPadding(FMargin(2.f, 0.f, 0.f, 0.f));
			ColumnSlot->SetHorizontalAlignment(HAlign_Right);
			ColumnSlot->SetVerticalAlignment(VAlign_Center);
		}
		else if (UVerticalBoxSlot* ColumnSlot = Cast<UVerticalBoxSlot>(ToolChipColumn->Slot))
		{
			UE_LOG(LogKodUI, Error, TEXT("ToolChipColumn is stacked with the minimap. Put it on the RIGHT of MinimapImage. Tools above the map are retired."));
			FSlateChildSize Auto(ESlateSizeRule::Automatic);
			ColumnSlot->SetSize(Auto);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(ToolChipColumn->Slot))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Right);
			OverlaySlot->SetVerticalAlignment(VAlign_Center);
			OverlaySlot->SetPadding(FMargin(0.f));
		}
		else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ToolChipColumn->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 1.f));
			CanvasSlot->SetAlignment(FVector2D(1.f, 0.5f));
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetOffsets(FMargin(0.f));
		}

		if (MinimapImage && ToolChipColumn->GetParent() && MinimapImage->GetParent() == ToolChipColumn->GetParent())
		{
			if (UPanelWidget* Bay = ToolChipColumn->GetParent())
			{
				const int32 ToolIndex = Bay->GetChildIndex(ToolChipColumn);
				const int32 MapIndex = Bay->GetChildIndex(MinimapImage);
				if (ToolIndex != INDEX_NONE && MapIndex != INDEX_NONE && ToolIndex < MapIndex && Cast<UVerticalBoxSlot>(ToolChipColumn->Slot))
				{
					UE_LOG(LogKodUI, Error, TEXT("Map tools are above MinimapImage. Move ToolChipColumn to the right of the map."));
				}
			}
		}
	}

	auto PlaceChip = [this](UWidget* Chip, const TCHAR* Name)
	{
		if (!Chip)
		{
			return;
		}
		if (Cast<UHorizontalBoxSlot>(Chip->Slot))
		{
			UE_LOG(LogKodUI, Error, TEXT("%s is in a horizontal tool strip. Parent it under ToolChipColumn, on the right of the minimap."), Name);
		}
		else if (ToolChipColumn && Chip->GetParent() != ToolChipColumn)
		{
			UE_LOG(LogKodUI, Warning, TEXT("%s should be a child of ToolChipColumn, in order: idle worker, army, ping."), Name);
		}
		else if (UVerticalBoxSlot* ChipSlot = Cast<UVerticalBoxSlot>(Chip->Slot))
		{
			FSlateChildSize Auto(ESlateSizeRule::Automatic);
			ChipSlot->SetSize(Auto);
			ChipSlot->SetPadding(FMargin(0.f, 2.f));
			ChipSlot->SetHorizontalAlignment(HAlign_Center);
			ChipSlot->SetVerticalAlignment(VAlign_Center);
		}
	};

	PlaceChip(Size_IdleWorker ? static_cast<UWidget*>(Size_IdleWorker) : static_cast<UWidget*>(Tool_IdleWorker), TEXT("Tool_IdleWorker"));
	PlaceChip(Size_Army ? static_cast<UWidget*>(Size_Army) : static_cast<UWidget*>(Tool_Army), TEXT("Tool_Army"));
	PlaceChip(Size_Ping ? static_cast<UWidget*>(Size_Ping) : static_cast<UWidget*>(Tool_Ping), TEXT("Tool_Ping"));

	if (!MinimapImage)
	{
		return;
	}
	if (UHorizontalBoxSlot* ImageSlot = Cast<UHorizontalBoxSlot>(MinimapImage->Slot))
	{
		FSlateChildSize Fill(ESlateSizeRule::Fill);
		Fill.Value = 1.f;
		ImageSlot->SetSize(Fill);
		ImageSlot->SetPadding(FMargin(0.f));
		ImageSlot->SetHorizontalAlignment(HAlign_Fill);
		ImageSlot->SetVerticalAlignment(VAlign_Fill);
	}
	else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(MinimapImage->Slot))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		OverlaySlot->SetPadding(FMargin(0.f));
	}
	else if (UVerticalBoxSlot* ImageSlot = Cast<UVerticalBoxSlot>(MinimapImage->Slot))
	{
		UE_LOG(LogKodUI, Error, TEXT("MinimapImage is in a vertical stack. Parent it beside ToolChipColumn so the map is flush and owns the bay."));
		FSlateChildSize Fill(ESlateSizeRule::Fill);
		Fill.Value = 1.f;
		ImageSlot->SetSize(Fill);
	}
	else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MinimapImage->Slot))
	{
		CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CanvasSlot->SetOffsets(FMargin(0.f));
		CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
		CanvasSlot->SetAutoSize(false);
	}

	if (ClockText && MinimapImage && ClockText->GetParent() && ClockText->GetParent() == MinimapImage->GetParent())
	{
		if (Cast<UVerticalBoxSlot>(ClockText->Slot) && Cast<UVerticalBoxSlot>(MinimapImage->Slot))
		{
			UE_LOG(LogKodUI, Warning, TEXT("ClockText is a row above the minimap. Keep it a thin tab on the frame so the map stays flush."));
		}
	}
}

void UKodMinimapWidget::ApplyClockText()
{
	if (!ClockText)
	{
		return;
	}
	ClockText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), ClockHours, ClockMinutes)));
	ClockText->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetActiveHudAccentColor()));
	ClockText->SetJustification(ETextJustify::Center);
}
