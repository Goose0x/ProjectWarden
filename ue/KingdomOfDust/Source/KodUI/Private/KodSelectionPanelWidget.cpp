#include "KodSelectionPanelWidget.h"
#include "KodLabeledButton.h"
#include "KodSelectionCellWidget.h"
#include "Style/KodUILayout.h"
#include "Style/KodUIStyle.h"

UKodSelectionCellWidget* UKodSelectionPanelWidget::GetCell(int32 Index) const
{
	return Cells.IsValidIndex(Index) ? Cells[Index] : nullptr;
}

void UKodSelectionPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	CacheCells();

	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		if (UKodSelectionCellWidget* Cell = Cells[Index])
		{
			Cell->SetCellIndex(Index);
			Cell->OnCellActivated.AddUniqueDynamic(this, &UKodSelectionPanelWidget::HandleCellClicked);
		}
	}
	for (UKodLabeledButton* Button : GroupButtons)
	{
		if (Button)
		{
			Button->OnLabeledClicked.AddUniqueDynamic(this, &UKodSelectionPanelWidget::HandleGroupClicked);
		}
	}
	ApplyGroupLabels();
}

void UKodSelectionPanelWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	CacheCells();
	ApplyGroupLabels();
	if (OverflowTabRail)
	{
		OverflowTabRail->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UKodSelectionPanelWidget::CacheCells()
{
	Cells = {
		Cell_00, Cell_01, Cell_02, Cell_03, Cell_04, Cell_05, Cell_06, Cell_07,
		Cell_08, Cell_09, Cell_10, Cell_11, Cell_12, Cell_13, Cell_14, Cell_15,
		Cell_16, Cell_17, Cell_18, Cell_19, Cell_20, Cell_21, Cell_22, Cell_23
	};
	GroupButtons = {
		Group_1, Group_2, Group_3, Group_4, Group_5, Group_6, Group_7, Group_8, Group_9, Group_0
	};
}

void UKodSelectionPanelWidget::ApplyGroupLabels()
{
	const TCHAR* Glyphs[] = { TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4"), TEXT("5"), TEXT("6"), TEXT("7"), TEXT("8"), TEXT("9"), TEXT("0") };
	for (int32 Visual = 0; Visual < GroupButtons.Num() && Visual < UE_ARRAY_COUNT(Glyphs); ++Visual)
	{
		if (UKodLabeledButton* Button = GroupButtons[Visual])
		{
			Button->SetStampLabel(FText::FromString(Glyphs[Visual]));
			Button->SetColorAndOpacity(UKodUIStyleLibrary::GetIronstockAmber());
		}
	}
}

void UKodSelectionPanelWidget::HandleGroupClicked(UKodLabeledButton* Button)
{
	const int32 Visual = GroupButtons.IndexOfByKey(Button);
	if (Visual != INDEX_NONE)
	{
		OnControlGroupClicked.Broadcast(KodUILayout::ControlGroupIndexFromVisual(Visual));
	}
}

void UKodSelectionPanelWidget::HandleCellClicked(UKodSelectionCellWidget* Cell)
{
	if (Cell)
	{
		OnSelectionCellClicked.Broadcast(Cell->GetCellIndex());
	}
}
