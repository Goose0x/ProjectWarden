#pragma once

#include "CommonUserWidget.h"
#include "KodSelectionPanelWidget.generated.h"

class UKodLabeledButton;
class UKodSelectionCellWidget;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodControlGroupClicked, int32, GroupIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodSelectionCellClicked, int32, CellIndex);

/**
 * Selection panel. Ironstock. No "CONTROL GROUPS" title.
 * Thin row Group_1 … Group_9, Group_0 over an 8×3 cell grid.
 * OverflowTabRail is the reserved page rail; it is not extra permanent cells.
 * Parent WBP: /Game/UI/HUD/WBP_SelectionPanel.
 *
 * Cells are row-major, 8 columns:
 *   Row 0: Cell_00 .. Cell_07
 *   Row 1: Cell_08 .. Cell_15
 *   Row 2: Cell_16 .. Cell_23
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodSelectionPanelWidget : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Selection")
	UKodSelectionCellWidget* GetCell(int32 Index) const;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Selection")
	FKodControlGroupClicked OnControlGroupClicked;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Selection")
	FKodSelectionCellClicked OnSelectionCellClicked;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	void CacheCells();
	void ApplyGroupLabels();

	UFUNCTION()
	void HandleGroupClicked(UKodLabeledButton* Button);

	UFUNCTION()
	void HandleCellClicked(UKodSelectionCellWidget* Cell);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UKodSelectionCellWidget>> Cells;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UKodLabeledButton>> GroupButtons;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodLabeledButton> Group_1;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodLabeledButton> Group_2;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodLabeledButton> Group_3;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodLabeledButton> Group_4;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodLabeledButton> Group_5;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodLabeledButton> Group_6;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodLabeledButton> Group_7;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodLabeledButton> Group_8;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodLabeledButton> Group_9;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodLabeledButton> Group_0;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_00;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_01;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_02;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_03;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_04;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_05;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_06;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_07;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_08;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_09;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_10;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_11;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_12;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_13;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_14;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_15;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_16;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_17;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_18;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_19;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_20;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_21;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_22;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Selection")
	TObjectPtr<UKodSelectionCellWidget> Cell_23;

	/** Thin rail on the panel edge. Shown when selection exceeds one page. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Selection")
	TObjectPtr<UWidget> OverflowTabRail;
};
