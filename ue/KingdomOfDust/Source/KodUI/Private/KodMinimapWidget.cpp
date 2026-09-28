#include "KodMinimapWidget.h"
#include "KodLabeledButton.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
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
	ApplyToolFloor(Size_IdleWorker);
	ApplyToolFloor(Size_Army);
	ApplyToolFloor(Size_Ping);
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

void UKodMinimapWidget::ApplyToolFloor(USizeBox* Box) const
{
	if (!Box)
	{
		return;
	}
	Box->SetMinDesiredWidth(KodUILayout::MapToolPreferredSizePx);
	Box->SetMinDesiredHeight(KodUILayout::MapToolPreferredSizePx);
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
