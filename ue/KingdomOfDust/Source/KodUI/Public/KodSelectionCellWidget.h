#pragma once

#include "CommonButtonBase.h"
#include "KodSelectionCellWidget.generated.h"

class UImage;
class UProgressBar;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodSelectionCellActivated, UKodSelectionCellWidget*, Cell);

/**
 * One portrait in the 8×3 selection grid.
 * Parent WBP: /Game/UI/HUD/WBP_SelectionCell. Name "Icon" and "HealthPip".
 */
UCLASS(Blueprintable)
class KODUI_API UKodSelectionCellWidget : public UCommonButtonBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Selection")
	void SetCellIndex(int32 Index);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Selection")
	int32 GetCellIndex() const { return CellIndex; }

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Selection")
	void SetOccupied(bool bInOccupied, bool bInPrimary);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Selection")
	void SetHealthFraction(float Fraction);

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Selection")
	FKodSelectionCellActivated OnCellActivated;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	void HandleInternalClicked();

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Selection")
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Selection")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Selection")
	bool bPrimary = false;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Selection")
	TObjectPtr<UImage> Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Selection")
	TObjectPtr<UProgressBar> HealthPip;
};
