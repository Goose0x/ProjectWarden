#include "KodCommandCardWidget.h"

void UKodCommandCardWidget::RefreshFromSelection()
{
	// Populate buttons from selected unit/building command sets (DataAssets)
}

void UKodCommandCardWidget::OnCommandButtonClicked(FName CommandId)
{
	(void)CommandId;
	// Enqueue Build / CastPower / etc. via UKodCommandSubsystem
}
