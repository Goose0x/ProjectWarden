#include "KodChatMenuClusterWidget.h"
#include "KodLabeledButton.h"
#include "Style/KodUIStyle.h"

void UKodChatMenuClusterWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_Chat)
	{
		Button_Chat->OnLabeledClicked.AddUniqueDynamic(this, &UKodChatMenuClusterWidget::HandleChat);
	}
	if (Button_Menu)
	{
		Button_Menu->OnLabeledClicked.AddUniqueDynamic(this, &UKodChatMenuClusterWidget::HandleMenu);
	}
	if (Button_Help)
	{
		Button_Help->OnLabeledClicked.AddUniqueDynamic(this, &UKodChatMenuClusterWidget::HandleHelp);
	}
}

void UKodChatMenuClusterWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	const FLinearColor Amber = UKodUIStyleLibrary::GetIronstockAmber();
	if (Button_Chat)
	{
		Button_Chat->SetStampLabel(NSLOCTEXT("KodUI", "HudChat", "CHAT"));
		Button_Chat->SetColorAndOpacity(Amber);
	}
	if (Button_Menu)
	{
		Button_Menu->SetStampLabel(NSLOCTEXT("KodUI", "HudMenu", "MENU"));
		Button_Menu->SetColorAndOpacity(Amber);
	}
	if (Button_Help)
	{
		Button_Help->SetStampLabel(NSLOCTEXT("KodUI", "HudHelp", "?"));
		Button_Help->SetColorAndOpacity(Amber);
	}
}

void UKodChatMenuClusterWidget::HandleChat(UKodLabeledButton* Button)
{
	(void)Button;
	OnChatClicked.Broadcast();
}

void UKodChatMenuClusterWidget::HandleMenu(UKodLabeledButton* Button)
{
	(void)Button;
	OnMenuClicked.Broadcast();
}

void UKodChatMenuClusterWidget::HandleHelp(UKodLabeledButton* Button)
{
	(void)Button;
	OnHelpClicked.Broadcast();
}
