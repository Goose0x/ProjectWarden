#include "KodFactionTileWidget.h"
#include "KodUI.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Style/KodUIStyle.h"

bool UKodFactionTileWidget::SetFaction(EKodVersusFaction InFaction)
{
	const bool bAllowed = InFaction == EKodVersusFaction::USA || InFaction == EKodVersusFaction::RSF;
	if (!bAllowed)
	{
		UE_LOG(LogKodUI, Error, TEXT("Faction tiles are USA and RSF only."));
		return false;
	}
	Faction = InFaction;
	if (LabelText)
	{
		LabelText->SetText(Faction == EKodVersusFaction::RSF
			? NSLOCTEXT("KodUI", "FactionRSF", "RSF")
			: NSLOCTEXT("KodUI", "FactionUSA", "USA"));
	}
	return true;
}

void UKodFactionTileWidget::SetTileState(EKodFactionTileState InState)
{
	TileState = InState;
	ApplyState();
}

void UKodFactionTileWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	OnClicked().AddUObject(this, &UKodFactionTileWidget::HandleInternalClicked);
}

void UKodFactionTileWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetFaction(Faction);
	ApplyState();
}

void UKodFactionTileWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (TileState != EKodFactionTileState::Selected && TileState != EKodFactionTileState::Disabled)
	{
		SetTileState(EKodFactionTileState::Hover);
	}
}

void UKodFactionTileWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (TileState == EKodFactionTileState::Hover)
	{
		SetTileState(EKodFactionTileState::Idle);
	}
}

void UKodFactionTileWidget::ApplyState()
{
	const FLinearColor Cyan = UKodUIStyleLibrary::GetCyanActive();
	const bool bDisabled = TileState == EKodFactionTileState::Disabled;
	SetIsEnabled(!bDisabled);

	float GlowOpacity = 0.f;
	FLinearColor Chrome = FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.35f);
	switch (TileState)
	{
	case EKodFactionTileState::Hover:
		GlowOpacity = 0.45f;
		Chrome = FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.75f);
		break;
	case EKodFactionTileState::Selected:
		GlowOpacity = 1.f;
		Chrome = Cyan;
		break;
	case EKodFactionTileState::Disabled:
		GlowOpacity = 0.f;
		Chrome = FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.15f);
		break;
	case EKodFactionTileState::Idle:
	default:
		break;
	}

	SetColorAndOpacity(Chrome);
	if (IconImage)
	{
		IconImage->SetColorAndOpacity(bDisabled ? Chrome : FLinearColor::White);
	}
	if (LabelText)
	{
		LabelText->SetColorAndOpacity(FSlateColor(bDisabled ? Chrome : UKodUIStyleLibrary::GetWhiteText()));
	}
	if (SelectionGlow)
	{
		SelectionGlow->SetColorAndOpacity(FLinearColor(Cyan.R, Cyan.G, Cyan.B, GlowOpacity));
		SelectionGlow->SetVisibility(GlowOpacity > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UKodFactionTileWidget::HandleInternalClicked()
{
	if (TileState == EKodFactionTileState::Disabled)
	{
		return;
	}
	OnTileClicked.Broadcast(this);
}
