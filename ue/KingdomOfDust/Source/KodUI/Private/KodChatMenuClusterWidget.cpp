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
	if (Button_Chat)
	{
		Button_Chat->SetStampLabel(NSLOCTEXT("KodUI", "HudChat", "CHAT"));
	}
	if (Button_Menu)
	{
		Button_Menu->SetStampLabel(NSLOCTEXT("KodUI", "HudMenu", "MENU"));
	}
	if (Button_Help)
	{
		Button_Help->SetStampLabel(NSLOCTEXT("KodUI", "HudHelp", "?"));
	}
	ApplyHudAccent();
}

void UKodChatMenuClusterWidget::ApplyHudAccent()
{
	const FLinearColor Accent = UKodUIStyleLibrary::GetActiveHudAccentColor();
	if (Button_Chat) { Button_Chat->SetColorAndOpacity(Accent); }
	if (Button_Menu) { Button_Menu->SetColorAndOpacity(Accent); }
	if (Button_Help) { Button_Help->SetColorAndOpacity(Accent); }
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
