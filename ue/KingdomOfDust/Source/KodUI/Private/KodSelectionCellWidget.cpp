#include "KodSelectionCellWidget.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Style/KodUIStyle.h"

void UKodSelectionCellWidget::SetCellIndex(int32 Index)
{
	CellIndex = Index;
}

void UKodSelectionCellWidget::SetOccupied(bool bInOccupied, bool bInPrimary)
{
	bOccupied = bInOccupied;
	bPrimary = bInPrimary;
	SetIsSelectable(true);
	SetIsToggleable(false);
	SetIsSelected(bPrimary);
	if (Icon)
	{
		Icon->SetVisibility(bOccupied ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}

void UKodSelectionCellWidget::SetHealthFraction(float Fraction)
{
	if (HealthPip)
	{
		HealthPip->SetPercent(FMath::Clamp(Fraction, 0.f, 1.f));
		HealthPip->SetFillColorAndOpacity(UKodUIStyleLibrary::GetHpGreen());
	}
}

void UKodSelectionCellWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	OnClicked().AddUObject(this, &UKodSelectionCellWidget::HandleInternalClicked);
}

void UKodSelectionCellWidget::HandleInternalClicked()
{
	OnCellActivated.Broadcast(this);
}

void UKodSelectionCellWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	SetColorAndOpacity(UKodUIStyleLibrary::GetActiveHudAccentColor());
	SetHealthFraction(bOccupied ? 1.f : 0.f);
}
