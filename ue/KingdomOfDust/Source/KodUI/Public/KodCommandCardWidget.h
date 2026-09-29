#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "KodCommandCardWidget.generated.h"

class UKodCommandSlotButton;
class UWidgetSwitcher;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodCommandRequested, FName, CommandId);

/**
 * 5×3 command card. Ironstock. No title text.
 * Top row is always Move / Stop / Hold / Patrol / Attack, shown as glyphs Q–T.
 * Build submenu replaces this card in place; Esc calls CloseBuildSubmenu.
 * Parent WBP: /Game/UI/HUD/WBP_CommandCard.
 *
 * Grid:
 *   Slot_Q Slot_W Slot_E Slot_R Slot_T
 *   Slot_A Slot_S Slot_D Slot_F Slot_G
 *   Slot_Z Slot_X Slot_C Slot_V Slot_B
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodCommandCardWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void RefreshFromSelection();

	UFUNCTION(BlueprintCallable, Category = "Kod|UI")
	void OnCommandButtonClicked(FName CommandId);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Command")
	void OpenBuildSubmenu();

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Command")
	void CloseBuildSubmenu();

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Command")
	void ApplyHudAccent();

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Command")
	bool IsBuildSubmenuOpen() const { return bBuildSubmenuOpen; }

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Command")
	FKodCommandRequested OnCommandRequested;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	void ApplyStandardOrders();
	void BindSlot(UKodCommandSlotButton* Slot);

	UFUNCTION()
	void HandleSlotClicked(UKodCommandSlotButton* InSlot);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Command")
	bool bBuildSubmenuOpen = false;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_Q;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_W;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_E;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_R;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_T;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_A;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_S;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_D;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_F;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_G;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_Z;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_X;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_C;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_V;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Command")
	TObjectPtr<UKodCommandSlotButton> Slot_B;

	/** Page 0 = the 5×3 grid. Page 1 = build submenu. Optional until the submenu WBP exists. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Command")
	TObjectPtr<UWidgetSwitcher> CardPages;
};
