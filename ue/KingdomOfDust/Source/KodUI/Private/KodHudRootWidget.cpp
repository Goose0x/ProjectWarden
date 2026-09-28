#include "KodHudRootWidget.h"
#include "KodUI.h"
#include "KodChatMenuClusterWidget.h"
#include "KodCommandCardWidget.h"
#include "KodMinimapWidget.h"
#include "KodPortraitFrameWidget.h"
#include "KodResourceBarWidget.h"
#include "KodSelectionPanelWidget.h"
#include "Style/KodUILayout.h"
#include "Style/KodUIStyle.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Spacer.h"

namespace KodHudRootPrivate
{
	void ApplyFill(UWidget* Widget, float Weight)
	{
		if (!Widget)
		{
			return;
		}
		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Widget->Slot))
		{
			FSlateChildSize Size(ESlateSizeRule::Fill);
			Size.Value = Weight;
			BoxSlot->SetSize(Size);
			BoxSlot->SetPadding(FMargin(0.f));
			BoxSlot->SetVerticalAlignment(VAlign_Fill);
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
		}
		else
		{
			UE_LOG(LogKodUI, Warning, TEXT("HUD column '%s' is not in HudColumns. Stamp widths were not applied."), *Widget->GetName());
		}
	}
}

UKodHudRootWidget::UKodHudRootWidget()
{
	Screen = EKodUIScreen::Hud;
	MaterialLanguage = EKodUIMaterialLanguage::Ironstock;
}

void UKodHudRootWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyStampProportions();
	ApplyFactionAccentTheme(AccentTheme);
}

void UKodHudRootWidget::ApplyFactionAccentTheme(EKodFactionAccentTheme Theme)
{
	if (!UKodUIStyleLibrary::SetActiveHudAccent(Theme))
	{
		return;
	}
	AccentTheme = Theme;
	// Id only. USA_Ironstock and RSF_RustOrange stay valid. Do not apply a palette table.
}

void UKodHudRootWidget::ApplyStampProportions()
{
	using namespace KodHudRootPrivate;

	if (IronstockPlate)
	{
		IronstockPlate->SetColorAndOpacity(UKodUIStyleLibrary::GetIronstockMetal());
	}

	if (UCanvasPanelSlot* BandSlot = Cast<UCanvasPanelSlot>(HudBand ? HudBand->Slot : nullptr))
	{
		const float Top = 1.f - KodUILayout::HudBandHeightFraction;
		BandSlot->SetAnchors(FAnchors(0.f, Top, 1.f, 1.f));
		BandSlot->SetOffsets(FMargin(0.f));
		BandSlot->SetAlignment(FVector2D(0.f, 0.f));
	}
	else if (HudBand)
	{
		UE_LOG(LogKodUI, Warning, TEXT("WBP_KodHUD: parent HudBand to a canvas so the lower 28%% band can be anchored."));
	}

	if (UCanvasPanelSlot* ResourceSlot = Cast<UCanvasPanelSlot>(ResourceBar ? ResourceBar->Slot : nullptr))
	{
		ResourceSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		ResourceSlot->SetAlignment(FVector2D(1.f, 0.f));
		ResourceSlot->SetAutoSize(true);
		ResourceSlot->SetPosition(FVector2D(-16.f, 16.f));
	}

	ApplyFill(Column_Minimap, static_cast<float>(KodUILayout::HudColumnWeightMinimap));
	ApplyFill(Column_Selection, static_cast<float>(KodUILayout::HudColumnWeightSelection));
	ApplyFill(Column_Portrait, static_cast<float>(KodUILayout::HudColumnWeightPortrait));
	ApplyFill(Column_Command, static_cast<float>(KodUILayout::HudColumnWeightCommand));
	ApplyFill(Column_Gutter, static_cast<float>(KodUILayout::HudColumnWeightGutter));

	if (HudColumns && Column_Gutter)
	{
		const int32 GutterIndex = HudColumns->GetChildIndex(Column_Gutter);
		const int32 Last = HudColumns->GetChildrenCount() - 1;
		if (GutterIndex != INDEX_NONE && GutterIndex != Last)
		{
			UE_LOG(LogKodUI, Warning, TEXT("HUD gutter is the trailing 5%% column. Do not take that width from selection."));
		}
	}

	if (ChatMenuCluster && CommandCard)
	{
		UPanelWidget* ChatParent = ChatMenuCluster->GetParent();
		UPanelWidget* CardParent = CommandCard->GetParent();
		if (ChatParent && ChatParent == CardParent)
		{
			if (ChatParent->GetChildIndex(ChatMenuCluster) > ChatParent->GetChildIndex(CommandCard))
			{
				UE_LOG(LogKodUI, Warning, TEXT("Chat/Menu cluster must sit above the command card."));
			}
		}
	}
}
