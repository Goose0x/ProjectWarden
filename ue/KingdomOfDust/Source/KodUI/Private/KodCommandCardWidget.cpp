#include "KodCommandCardWidget.h"
#include "KodCommandSlotButton.h"
#include "Components/WidgetSwitcher.h"
#include "Style/KodUIStyle.h"

namespace KodCommandCardPrivate
{
	const FName IdMove(TEXT("Move"));
	const FName IdStop(TEXT("Stop"));
	const FName IdHold(TEXT("Hold"));
	const FName IdPatrol(TEXT("Patrol"));
	const FName IdAttack(TEXT("Attack"));
}

void UKodCommandCardWidget::RefreshFromSelection()
{
	// Lower rows fill from the selected unit/building command set.
	// The top row stays the standard orders.
	ApplyStandardOrders();
}

void UKodCommandCardWidget::OnCommandButtonClicked(FName CommandId)
{
	OnCommandRequested.Broadcast(CommandId);
}

void UKodCommandCardWidget::OpenBuildSubmenu()
{
	bBuildSubmenuOpen = true;
	if (CardPages)
	{
		CardPages->SetActiveWidgetIndex(1);
	}
}

void UKodCommandCardWidget::ApplyHudAccent()
{
	const FLinearColor Accent = UKodUIStyleLibrary::GetActiveHudAccentColor();
	const TArray<UKodCommandSlotButton*> Slots = {
		Slot_Q, Slot_W, Slot_E, Slot_R, Slot_T,
		Slot_A, Slot_S, Slot_D, Slot_F, Slot_G,
		Slot_Z, Slot_X, Slot_C, Slot_V, Slot_B
	};
	for (UKodCommandSlotButton* Slot : Slots)
	{
		if (Slot)
		{
			Slot->SetColorAndOpacity(Accent);
		}
	}
}

void UKodCommandCardWidget::CloseBuildSubmenu()
{
	bBuildSubmenuOpen = false;
	if (CardPages)
	{
		CardPages->SetActiveWidgetIndex(0);
	}
}

void UKodCommandCardWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BindSlot(Slot_Q);
	BindSlot(Slot_W);
	BindSlot(Slot_E);
	BindSlot(Slot_R);
	BindSlot(Slot_T);
	BindSlot(Slot_A);
	BindSlot(Slot_S);
	BindSlot(Slot_D);
	BindSlot(Slot_F);
	BindSlot(Slot_G);
	BindSlot(Slot_Z);
	BindSlot(Slot_X);
	BindSlot(Slot_C);
	BindSlot(Slot_V);
	BindSlot(Slot_B);
	ApplyStandardOrders();
}

void UKodCommandCardWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyStandardOrders();
	if (CardPages && !bBuildSubmenuOpen)
	{
		CardPages->SetActiveWidgetIndex(0);
	}
}

void UKodCommandCardWidget::ApplyStandardOrders()
{
	using namespace KodCommandCardPrivate;
	auto SetTop = [](UKodCommandSlotButton* Slot, FName Id, const TCHAR* Glyph, const FText& OrderName)
	{
		if (Slot)
		{
			Slot->SetSlotIdentity(Id, FText::FromString(Glyph), OrderName);
			Slot->SetChargeCount(INDEX_NONE);
		}
	};
	auto SetGlyph = [](UKodCommandSlotButton* Slot, const TCHAR* Glyph)
	{
		if (Slot)
		{
			Slot->SetSlotIdentity(NAME_None, FText::FromString(Glyph), FText::GetEmpty());
			Slot->SetChargeCount(INDEX_NONE);
		}
	};

	SetTop(Slot_Q, IdMove, TEXT("Q"), NSLOCTEXT("KodUI", "OrderMove", "Move"));
	SetTop(Slot_W, IdStop, TEXT("W"), NSLOCTEXT("KodUI", "OrderStop", "Stop"));
	SetTop(Slot_E, IdHold, TEXT("E"), NSLOCTEXT("KodUI", "OrderHold", "Hold"));
	SetTop(Slot_R, IdPatrol, TEXT("R"), NSLOCTEXT("KodUI", "OrderPatrol", "Patrol"));
	SetTop(Slot_T, IdAttack, TEXT("T"), NSLOCTEXT("KodUI", "OrderAttack", "Attack"));

	SetGlyph(Slot_A, TEXT("A"));
	SetGlyph(Slot_S, TEXT("S"));
	SetGlyph(Slot_D, TEXT("D"));
	SetGlyph(Slot_F, TEXT("F"));
	SetGlyph(Slot_G, TEXT("G"));
	SetGlyph(Slot_Z, TEXT("Z"));
	SetGlyph(Slot_X, TEXT("X"));
	SetGlyph(Slot_C, TEXT("C"));
	SetGlyph(Slot_V, TEXT("V"));
	SetGlyph(Slot_B, TEXT("B"));
}

void UKodCommandCardWidget::BindSlot(UKodCommandSlotButton* Slot)
{
	if (Slot)
	{
		Slot->OnSlotClicked.AddUniqueDynamic(this, &UKodCommandCardWidget::HandleSlotClicked);
	}
}

void UKodCommandCardWidget::HandleSlotClicked(UKodCommandSlotButton* Slot)
{
	if (Slot)
	{
		OnCommandButtonClicked(Slot->GetCommandId());
	}
}
