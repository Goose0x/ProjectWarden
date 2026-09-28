#include "KodMinimapWidget.h"
#include "KodUI.h"
#include "KodLabeledButton.h"
#include "CommonTextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
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

	if (ToolChipStrip)
	{
		if (UVerticalBoxSlot* StripSlot = Cast<UVerticalBoxSlot>(ToolChipStrip->Slot))
		{
			FSlateChildSize Auto(ESlateSizeRule::Automatic);
			StripSlot->SetSize(Auto);
			StripSlot->SetPadding(FMargin(0.f));
			StripSlot->SetHorizontalAlignment(HAlign_Fill);
			StripSlot->SetVerticalAlignment(VAlign_Center);
		}
		else if (UHorizontalBoxSlot* StripSlot = Cast<UHorizontalBoxSlot>(ToolChipStrip->Slot))
		{
			FSlateChildSize Auto(ESlateSizeRule::Automatic);
			StripSlot->SetSize(Auto);
			StripSlot->SetVerticalAlignment(VAlign_Center);
		}
		else if (UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(ToolChipStrip->Slot))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Left);
			OverlaySlot->SetVerticalAlignment(VAlign_Top);
			OverlaySlot->SetPadding(FMargin(0.f));
		}
		else if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ToolChipStrip->Slot))
		{
			CanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
			CanvasSlot->SetAlignment(FVector2D(0.f, 0.f));
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetOffsets(FMargin(0.f));
		}
	}

	auto PlaceChip = [this](UWidget* Chip, const TCHAR* Name)
	{
		if (!Chip)
		{
			return;
		}
		if (UVerticalBoxSlot* StackSlot = Cast<UVerticalBoxSlot>(Chip->Slot))
		{
			UE_LOG(LogKodUI, Warning, TEXT("%s is in a vertical tool stack. Tool placement is HOLD pending the updated template plate. Do not treat the header strip as the lock."), Name);
			FSlateChildSize Auto(ESlateSizeRule::Automatic);
			StackSlot->SetSize(Auto);
		}
		else if (ToolChipStrip && Chip->GetParent() != ToolChipStrip)
		{
			UE_LOG(LogKodUI, Warning, TEXT("%s is outside ToolChipStrip. That header strip is not the locked plate. Tool placement is HOLD pending the updated template."), Name);
		}
		else if (UHorizontalBoxSlot* ChipSlot = Cast<UHorizontalBoxSlot>(Chip->Slot))
		{
			FSlateChildSize Auto(ESlateSizeRule::Automatic);
			ChipSlot->SetSize(Auto);
			ChipSlot->SetPadding(FMargin(2.f, 0.f));
			ChipSlot->SetVerticalAlignment(VAlign_Center);
			ChipSlot->SetHorizontalAlignment(HAlign_Center);
		}
	};

	PlaceChip(Size_IdleWorker ? static_cast<UWidget*>(Size_IdleWorker) : static_cast<UWidget*>(Tool_IdleWorker), TEXT("Tool_IdleWorker"));
	PlaceChip(Size_Army ? static_cast<UWidget*>(Size_Army) : static_cast<UWidget*>(Tool_Army), TEXT("Tool_Army"));
	PlaceChip(Size_Ping ? static_cast<UWidget*>(Size_Ping) : static_cast<UWidget*>(Tool_Ping), TEXT("Tool_Ping"));

	if (!MinimapImage)
	{
		return;
	}
	if (UVerticalBoxSlot* ImageSlot = Cast<UVerticalBoxSlot>(MinimapImage->Slot))
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
	else if (!Cast<UCanvasPanelSlot>(MinimapImage->Slot))
	{
		UE_LOG(LogKodUI, Warning, TEXT("Parent MinimapImage under the chip header so it fills the left bay. Do not leave a fat tool stack beside the map."));
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
