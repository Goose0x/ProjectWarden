#include "KodMainMenuWidget.h"
#include "KodIdentityStripWidget.h"
#include "KodLabeledButton.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Style/KodUIStyle.h"

UKodMainMenuWidget::UKodMainMenuWidget()
{
	Screen = EKodUIScreen::MainMenu;
	MaterialLanguage = EKodUIMaterialLanguage::CyanGlass;
}

void UKodMainMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (Button_PlayVersus) { Button_PlayVersus->OnLabeledClicked.AddUniqueDynamic(this, &UKodMainMenuWidget::HandlePlayVersus); }
	if (Nav_Training) { Nav_Training->OnLabeledClicked.AddUniqueDynamic(this, &UKodMainMenuWidget::HandleTraining); }
	if (Nav_Campaign) { Nav_Campaign->OnLabeledClicked.AddUniqueDynamic(this, &UKodMainMenuWidget::HandleCampaign); }
	if (Nav_Coop) { Nav_Coop->OnLabeledClicked.AddUniqueDynamic(this, &UKodMainMenuWidget::HandleCoop); }
	if (Nav_Armory) { Nav_Armory->OnLabeledClicked.AddUniqueDynamic(this, &UKodMainMenuWidget::HandleArmory); }
	if (Nav_Options) { Nav_Options->OnLabeledClicked.AddUniqueDynamic(this, &UKodMainMenuWidget::HandleOptions); }
	if (Nav_Exit) { Nav_Exit->OnLabeledClicked.AddUniqueDynamic(this, &UKodMainMenuWidget::HandleExit); }
	if (Util_Friends) { Util_Friends->OnLabeledClicked.AddUniqueDynamic(this, &UKodMainMenuWidget::HandleFriends); }
	if (Util_Help) { Util_Help->OnLabeledClicked.AddUniqueDynamic(this, &UKodMainMenuWidget::HandleHelp); }
	if (Util_Menu) { Util_Menu->OnLabeledClicked.AddUniqueDynamic(this, &UKodMainMenuWidget::HandleMenu); }
}

void UKodMainMenuWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyStampCopy();
}

void UKodMainMenuWidget::ApplyStampCopy()
{
	if (HeroArt)
	{
		HeroArt->SetColorAndOpacity(FLinearColor::White);
	}
	if (GlassPlate)
	{
		GlassPlate->SetColorAndOpacity(UKodUIStyleLibrary::GetPanelBlue());
	}

	TintButton(Button_PlayVersus, true);
	if (Button_PlayVersus)
	{
		Button_PlayVersus->SetStampLabel(NSLOCTEXT("KodUI", "PlayVersus", "PLAY VERSUS"));
	}
	if (Caption_VersusLobby)
	{
		Caption_VersusLobby->SetText(NSLOCTEXT("KodUI", "VersusLobbyCaption", "Versus lobby"));
		Caption_VersusLobby->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetCyanActive()));
	}

	if (Nav_Training) { Nav_Training->SetStampLabel(NSLOCTEXT("KodUI", "NavTraining", "TRAINING")); }
	if (Nav_Campaign) { Nav_Campaign->SetStampLabel(NSLOCTEXT("KodUI", "NavCampaign", "CAMPAIGN")); }
	if (Nav_Coop) { Nav_Coop->SetStampLabel(NSLOCTEXT("KodUI", "NavCoop", "CO-OP")); }
	if (Nav_Armory) { Nav_Armory->SetStampLabel(NSLOCTEXT("KodUI", "NavArmory", "ARMORY")); }
	if (Nav_Options) { Nav_Options->SetStampLabel(NSLOCTEXT("KodUI", "NavOptions", "OPTIONS")); }
	if (Nav_Exit) { Nav_Exit->SetStampLabel(NSLOCTEXT("KodUI", "NavExit", "EXIT")); }
	TintButton(Nav_Training, false);
	TintButton(Nav_Campaign, false);
	TintButton(Nav_Coop, false);
	TintButton(Nav_Armory, false);
	TintButton(Nav_Options, false);
	TintButton(Nav_Exit, false);

	if (Util_Friends) { Util_Friends->SetStampLabel(NSLOCTEXT("KodUI", "UtilFriends", "FRIENDS")); }
	if (Util_Help) { Util_Help->SetStampLabel(NSLOCTEXT("KodUI", "UtilHelp", "HELP")); }
	if (Util_Menu) { Util_Menu->SetStampLabel(NSLOCTEXT("KodUI", "UtilMenu", "MENU")); }
	TintButton(Util_Friends, false);
	TintButton(Util_Help, false);
	TintButton(Util_Menu, false);

	const FSlateColor Text(UKodUIStyleLibrary::GetWhiteText());
	if (BriefingTitle)
	{
		BriefingTitle->SetText(NSLOCTEXT("KodUI", "BriefingTitle", "BRIEFING"));
		BriefingTitle->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetCyanActive()));
	}
	if (Briefing_Patch)
	{
		Briefing_Patch->SetText(NSLOCTEXT("KodUI", "BriefingPatch", "Patch 0.9.2 notes"));
		Briefing_Patch->SetColorAndOpacity(Text);
	}
	if (Briefing_FeaturedMap)
	{
		Briefing_FeaturedMap->SetText(NSLOCTEXT("KodUI", "BriefingMap", "Featured map: Desert Rail"));
		Briefing_FeaturedMap->SetColorAndOpacity(Text);
	}
	if (Briefing_Season)
	{
		Briefing_Season->SetText(NSLOCTEXT("KodUI", "BriefingSeason", "Season 3 live"));
		Briefing_Season->SetColorAndOpacity(Text);
	}
	if (Footer_Version)
	{
		Footer_Version->SetText(NSLOCTEXT("KodUI", "MainMenuVersion", "v1 paint"));
		Footer_Version->SetColorAndOpacity(Text);
	}
	if (Footer_Online)
	{
		Footer_Online->SetText(NSLOCTEXT("KodUI", "FooterOnline", "ONLINE"));
		Footer_Online->SetColorAndOpacity(FSlateColor(UKodUIStyleLibrary::GetCyanActive()));
	}
}

void UKodMainMenuWidget::TintButton(UKodLabeledButton* Button, bool bCommit) const
{
	if (!Button)
	{
		return;
	}
	Button->SetColorAndOpacity(bCommit
		? UKodUIStyleLibrary::GetCommitColor(EKodUIMaterialLanguage::CyanGlass)
		: UKodUIStyleLibrary::GetCyanActive());
}

void UKodMainMenuWidget::HandlePlayVersus(UKodLabeledButton* Button)
{
	(void)Button;
	OnMainMenuAction.Broadcast(EKodMainMenuAction::PlayVersus);
}

void UKodMainMenuWidget::HandleTraining(UKodLabeledButton* Button)
{
	(void)Button;
	OnMainMenuAction.Broadcast(EKodMainMenuAction::Training);
}

void UKodMainMenuWidget::HandleCampaign(UKodLabeledButton* Button)
{
	(void)Button;
	OnMainMenuAction.Broadcast(EKodMainMenuAction::Campaign);
}

void UKodMainMenuWidget::HandleCoop(UKodLabeledButton* Button)
{
	(void)Button;
	OnMainMenuAction.Broadcast(EKodMainMenuAction::Coop);
}

void UKodMainMenuWidget::HandleArmory(UKodLabeledButton* Button)
{
	(void)Button;
	OnMainMenuAction.Broadcast(EKodMainMenuAction::Armory);
}

void UKodMainMenuWidget::HandleOptions(UKodLabeledButton* Button)
{
	(void)Button;
	OnMainMenuAction.Broadcast(EKodMainMenuAction::Options);
}

void UKodMainMenuWidget::HandleExit(UKodLabeledButton* Button)
{
	(void)Button;
	OnMainMenuAction.Broadcast(EKodMainMenuAction::Exit);
}

void UKodMainMenuWidget::HandleFriends(UKodLabeledButton* Button)
{
	(void)Button;
	OnMainMenuAction.Broadcast(EKodMainMenuAction::Friends);
}

void UKodMainMenuWidget::HandleHelp(UKodLabeledButton* Button)
{
	(void)Button;
	OnMainMenuAction.Broadcast(EKodMainMenuAction::Help);
}

void UKodMainMenuWidget::HandleMenu(UKodLabeledButton* Button)
{
	(void)Button;
	OnMainMenuAction.Broadcast(EKodMainMenuAction::Menu);
}
