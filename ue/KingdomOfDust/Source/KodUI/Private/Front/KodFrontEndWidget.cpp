#include "Front/KodFrontEndWidget.h"

#include "CommonTextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ButtonSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Game/KodJadeiteTunerWidget.h"
#include "KodOnlineSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Style/KodUIStyle.h"
#include "TimerManager.h"
#include "Blueprint/WidgetTree.h"

namespace
{
	FName FrontAction(const TCHAR* Name)
	{
		return FName(Name);
	}

	FLinearColor TeamColorAt(int32 Index)
	{
		return FKodSkirmishSetup::TeamSwatch(Index);
	}

	const TCHAR* StartName(int32 Index)
	{
		return Index == 0 ? TEXT("North") : TEXT("South");
	}
}

void UKodFrontButton::BindMenu(UKodFrontEndWidget* MenuWidget, FName InAction)
{
	MenuOwner = MenuWidget;
	MenuAction = InAction;
	OnClicked.AddUniqueDynamic(this, &UKodFrontButton::ForwardClick);
}

void UKodFrontButton::ForwardClick()
{
	if (MenuOwner)
	{
		MenuOwner->HandleAction(MenuAction);
	}
}

TSharedRef<SWidget> UKodFrontEndWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildShell();
	}
	return Super::RebuildWidget();
}

void UKodFrontEndWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	FKodSkirmishSetup::GetMaps(Maps);
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildShell();
	}
	bDeferUi = false;
	Show(EKodFrontPage::Menu);
	bDeferUi = true;
}

void UKodFrontEndWidget::BuildShell()
{
	Plate = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Plate"));
	Plate->SetBrushColor(UKodUIStyleLibrary::GetIronstockMetal());
	Body = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Body"));
	Plate->SetContent(Body);
	WidgetTree->RootWidget = Plate;
}

void UKodFrontEndWidget::Show(EKodFrontPage NextPage)
{
	Page = NextPage;
	Refresh();
}

void UKodFrontEndWidget::Refresh()
{
	if (!bDeferUi)
	{
		RebuildPage();
		return;
	}
	if (bRefreshQueued)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		RebuildPage();
		return;
	}
	bRefreshQueued = true;
	TWeakObjectPtr<UKodFrontEndWidget> Self(this);
	World->GetTimerManager().SetTimerForNextTick([Self]()
	{
		if (UKodFrontEndWidget* Widget = Self.Get())
		{
			Widget->bRefreshQueued = false;
			Widget->RebuildPage();
		}
	});
}

void UKodFrontEndWidget::AddLine(UVerticalBox* Box, const FText& Line, bool bDim)
{
	UCommonTextBlock* Text = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass());
	Text->SetText(Line);
	const FLinearColor Ink = bDim
		? UKodUIStyleLibrary::GetIronstockAmber().CopyWithNewOpacity(0.45f)
		: UKodUIStyleLibrary::GetWhiteText();
	Text->SetColorAndOpacity(FSlateColor(Ink));
	Box->AddChildToVerticalBox(Text);
}

void UKodFrontEndWidget::AddButton(UVerticalBox* Box, const FText& Label, FName Action, bool bEnabled, const FLinearColor* Fill)
{
	UKodFrontButton* Button = WidgetTree->ConstructWidget<UKodFrontButton>(UKodFrontButton::StaticClass());
	UCommonTextBlock* Text = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass());
	Text->SetText(Label);
	const bool bTinted = bEnabled && Fill != nullptr;
	const FLinearColor Ink = bTinted
		? UKodUIStyleLibrary::GetWhiteText()
		: (bEnabled
			? UKodUIStyleLibrary::GetIronstockAmber()
			: UKodUIStyleLibrary::GetIronstockAmber().CopyWithNewOpacity(0.35f));
	Text->SetColorAndOpacity(FSlateColor(Ink));
	Button->AddChild(Text);
	Button->SetIsEnabled(bEnabled);
	if (bTinted)
	{
		Button->SetBackgroundColor(*Fill);
	}
	else
	{
		Button->SetBackgroundColor(bEnabled ? UKodUIStyleLibrary::GetHpGreen() : UKodUIStyleLibrary::GetAlertRed());
	}
	Button->BindMenu(this, Action);
	if (UVerticalBoxSlot* ButtonSlot = Box->AddChildToVerticalBox(Button))
	{
		ButtonSlot->SetPadding(FMargin(0.f, 6.f));
	}
}

void UKodFrontEndWidget::AddPortrait(UHorizontalBox* Row, const FText& Title, const FText& Mark, FName Action, FLinearColor Fill, bool bEnabled, bool bSelected)
{
	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(160.f);
	Size->SetHeightOverride(160.f);

	UKodFrontButton* Button = WidgetTree->ConstructWidget<UKodFrontButton>(UKodFrontButton::StaticClass());
	Button->SetIsEnabled(bEnabled);
	Button->SetBackgroundColor(bSelected ? UKodUIStyleLibrary::GetWhiteText() : UKodUIStyleLibrary::GetIronstockMetal());
	Button->BindMenu(this, Action);

	UBorder* Emblem = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	const FLinearColor PlateColor = bEnabled ? Fill : Fill.CopyWithNewOpacity(0.35f);
	Emblem->SetBrushColor(PlateColor);
	Emblem->SetPadding(FMargin(8.f));

	UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UCommonTextBlock* MarkText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass());
	MarkText->SetText(Mark);
	UCommonTextBlock* TitleText = WidgetTree->ConstructWidget<UCommonTextBlock>(UCommonTextBlock::StaticClass());
	TitleText->SetText(Title);
	const FLinearColor Ink = bEnabled
		? UKodUIStyleLibrary::GetWhiteText()
		: UKodUIStyleLibrary::GetWhiteText().CopyWithNewOpacity(0.45f);
	MarkText->SetColorAndOpacity(FSlateColor(Ink));
	TitleText->SetColorAndOpacity(FSlateColor(Ink));
	Stack->AddChildToVerticalBox(MarkText);
	Stack->AddChildToVerticalBox(TitleText);
	Emblem->SetContent(Stack);
	if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Button->AddChild(Emblem)))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Fill);
		ButtonSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (USizeBoxSlot* SizeSlot = Cast<USizeBoxSlot>(Size->AddChild(Button)))
	{
		SizeSlot->SetHorizontalAlignment(HAlign_Fill);
		SizeSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UHorizontalBoxSlot* SizeSlot = Row->AddChildToHorizontalBox(Size))
	{
		SizeSlot->SetPadding(FMargin(8.f, 8.f));
	}
}

FString UKodFrontEndWidget::MapLabel() const
{
	if (!Maps.IsValidIndex(MapCursor))
	{
		return TEXT("Faultline Training");
	}
	const FKodLobbyMap& LobbyMap = Maps[MapCursor];
	return LobbyMap.bPlayable ? LobbyMap.Label : LobbyMap.Label + TEXT(" (not playable)");
}

void UKodFrontEndWidget::RebuildPage()
{
	if (!Body)
	{
		return;
	}
	Body->ClearChildren();
	AddLine(Body, FText::FromString(TEXT("Kingdom of Dust")), false);
	if (Page == EKodFrontPage::Menu)
	{
		AddButton(Body, FText::FromString(TEXT("Campaign")), FrontAction(TEXT("Campaign")), true);
		AddButton(Body, FText::FromString(TEXT("Skirmish")), FrontAction(TEXT("Skirmish")), true);
		bool bOnline = false;
		if (UGameInstance* Instance = GetGameInstance())
		{
			if (UKodOnlineSubsystem* Online = Instance->GetSubsystem<UKodOnlineSubsystem>())
			{
				bOnline = Online->GetService().IsRemoteBackend();
			}
		}
		AddButton(Body, FText::FromString(TEXT("Multiplayer")), FrontAction(TEXT("Multiplayer")), false);
		AddLine(Body, FText::FromString(bOnline ? TEXT("Online backend ready") : TEXT("Multiplayer coming soon")), true);
		AddButton(Body, FText::FromString(TEXT("Options")), FrontAction(TEXT("Options")), true);
		AddButton(Body, FText::FromString(TEXT("Quit")), FrontAction(TEXT("Quit")), true);
		return;
	}
	if (Page == EKodFrontPage::Faction)
	{
		AddLine(Body, FText::FromString(bCampaignPick ? TEXT("Campaign faction") : TEXT("Faction")), false);
		UHorizontalBox* Portraits = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		const FKodHudAccentColors VanguardAccent = UKodUIStyleLibrary::GetHudAccentColors(EKodFactionAccentTheme::USA_TanGreenGold);
		const FKodHudAccentColors ScinderAccent = UKodUIStyleLibrary::GetHudAccentColors(EKodFactionAccentTheme::RSF_MetalStoneOrangeArch);
		AddPortrait(Portraits, FText::FromString(TEXT("Vanguard")), FText::FromString(TEXT("V")), FrontAction(TEXT("FacVan")), VanguardAccent.Field, true, Setup.PlayerFaction == EKodRosterFaction::Vanguard);
		AddPortrait(Portraits, FText::FromString(TEXT("Scinder")), FText::FromString(TEXT("S")), FrontAction(TEXT("FacScn")), ScinderAccent.Field, true, Setup.PlayerFaction == EKodRosterFaction::Scinder);
		AddPortrait(Portraits, FText::FromString(TEXT("Dynasty")), FText::FromString(TEXT("D")), FrontAction(TEXT("Dynasty")), UKodUIStyleLibrary::GetIronstockMetal(), false, false);
		Body->AddChildToVerticalBox(Portraits);
		AddLine(Body, FText::FromString(TEXT("Dynasty is not available")), true);
		AddButton(Body, FText::FromString(TEXT("Random")), FrontAction(TEXT("FacRnd")), true);
		AddLine(Body, FText::FromString(FString::Printf(TEXT("Selected %s"), *FKodSkirmishSetup::FactionName(Setup.PlayerFaction))), false);
		AddButton(Body, FText::FromString(bCampaignPick ? TEXT("Begin training") : TEXT("Continue")), FrontAction(TEXT("FacOk")), true);
		AddButton(Body, FText::FromString(TEXT("Back")), FrontAction(TEXT("Back")), true);
		return;
	}
	if (Page == EKodFrontPage::Lobby)
	{
		AddLine(Body, FText::FromString(TEXT("Versus")), false);
		const bool bMapOk = Maps.IsValidIndex(MapCursor) && Maps[MapCursor].bPlayable;
		AddButton(Body, FText::FromString(FString::Printf(TEXT("Map: %s"), *MapLabel())), FrontAction(TEXT("Map")), true);
		AddButton(Body, FText::FromString(TEXT("You")), FrontAction(TEXT("Human")), false);
		AddButton(Body, FText::FromString(FString::Printf(TEXT("Faction: %s"), *FKodSkirmishSetup::FactionName(Setup.PlayerFaction))), FrontAction(TEXT("PFac")), true);
		const FLinearColor PlayerSwatch = TeamColorAt(Setup.PlayerColor);
		AddButton(Body, FText::FromString(FString::Printf(TEXT("Colour %d"), Setup.PlayerColor)), FrontAction(TEXT("PCol")), true, &PlayerSwatch);
		AddButton(Body, FText::FromString(FString::Printf(TEXT("Start: %s"), StartName(Setup.PlayerStart))), FrontAction(TEXT("StartPos")), true);
		AddButton(Body, FText::FromString(FString::Printf(TEXT("AI: %s"), *FKodSkirmishSetup::DifficultyName(Setup.Difficulty))), FrontAction(TEXT("Diff")), true);
		AddButton(Body, FText::FromString(FString::Printf(TEXT("AI faction: %s"), *FKodSkirmishSetup::FactionName(Setup.AiFaction))), FrontAction(TEXT("AFac")), true);
		const FLinearColor AiSwatch = TeamColorAt(Setup.AiColor);
		AddButton(Body, FText::FromString(FString::Printf(TEXT("AI colour %d"), Setup.AiColor)), FrontAction(TEXT("ACol")), true, &AiSwatch);
		AddLine(Body, FText::FromString(FString::Printf(TEXT("AI start: %s"), StartName(Setup.AiStart))), true);
		AddLine(Body, FText::FromString(TEXT("Cheats stay on for offline skirmish")), true);
		AddButton(Body, FText::FromString(TEXT("Start")), FrontAction(TEXT("Begin")), bMapOk);
		AddButton(Body, FText::FromString(TEXT("Back")), FrontAction(TEXT("Back")), true);
		return;
	}
	AddLine(Body, FText::FromString(TEXT("Options")), false);
	AddLine(Body, FText::FromString(TEXT("Camera: kod.Cam.*")), true);
	AddLine(Body, FText::FromString(TEXT("Audio: not wired")), true);
	AddLine(Body, FText::FromString(TEXT("Keybinds: not wired")), true);
	AddButton(Body, FText::FromString(TEXT("Jadeite tuner")), FrontAction(TEXT("Jadeite")), true);
	AddButton(Body, FText::FromString(TEXT("Back")), FrontAction(TEXT("Back")), true);
}

void UKodFrontEndWidget::HandleAction(FName Action)
{
	if (Action == FrontAction(TEXT("Campaign")))
	{
		bCampaignPick = true;
		Setup.Mode = EKodMatchMode::Campaign;
		Setup.MapId = TEXT("Faultline");
		Show(EKodFrontPage::Faction);
		return;
	}
	if (Action == FrontAction(TEXT("Skirmish")))
	{
		bCampaignPick = false;
		Setup.Mode = EKodMatchMode::OfflineSkirmish;
		Setup.bEnableAi = true;
		if (UGameInstance* Instance = GetGameInstance())
		{
			if (UKodOnlineSubsystem* Online = Instance->GetSubsystem<UKodOnlineSubsystem>())
			{
				Online->GetService().HostOfflineLobby();
			}
		}
		Show(EKodFrontPage::Lobby);
		return;
	}
	if (Action == FrontAction(TEXT("Options")))
	{
		Show(EKodFrontPage::Options);
		return;
	}
	if (Action == FrontAction(TEXT("Quit")))
	{
		UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
		return;
	}
	if (Action == FrontAction(TEXT("Back")))
	{
		Show(EKodFrontPage::Menu);
		return;
	}
	if (Action == FrontAction(TEXT("FacVan")))
	{
		Setup.PlayerFaction = EKodRosterFaction::Vanguard;
		Refresh();
		return;
	}
	if (Action == FrontAction(TEXT("FacScn")))
	{
		Setup.PlayerFaction = EKodRosterFaction::Scinder;
		Refresh();
		return;
	}
	if (Action == FrontAction(TEXT("FacRnd")))
	{
		Setup.PlayerFaction = EKodRosterFaction::Random;
		Setup.Seed = FMath::RandRange(1, 100000);
		Refresh();
		return;
	}
	if (Action == FrontAction(TEXT("FacOk")))
	{
		if (bCampaignPick)
		{
			Travel(Setup);
			return;
		}
		Show(EKodFrontPage::Lobby);
		return;
	}
	if (Action == FrontAction(TEXT("Map")))
	{
		CycleMap();
		return;
	}
	if (Action == FrontAction(TEXT("Diff")))
	{
		CycleDifficulty();
		return;
	}
	if (Action == FrontAction(TEXT("PFac")))
	{
		CyclePlayerFaction();
		return;
	}
	if (Action == FrontAction(TEXT("AFac")))
	{
		CycleAiFaction();
		return;
	}
	if (Action == FrontAction(TEXT("PCol")))
	{
		CycleColor(true);
		return;
	}
	if (Action == FrontAction(TEXT("ACol")))
	{
		CycleColor(false);
		return;
	}
	if (Action == FrontAction(TEXT("StartPos")))
	{
		ToggleStart();
		return;
	}
	if (Action == FrontAction(TEXT("Begin")))
	{
		if (Maps.IsValidIndex(MapCursor))
		{
			Setup.MapId = Maps[MapCursor].Id;
		}
		Setup.Mode = EKodMatchMode::OfflineSkirmish;
		Setup.bEnableAi = true;
		Travel(Setup);
		return;
	}
	if (Action == FrontAction(TEXT("Jadeite")))
	{
		if (!JadeitePanel)
		{
			JadeitePanel = CreateWidget<UKodJadeiteTunerWidget>(GetOwningPlayer(), UKodJadeiteTunerWidget::StaticClass());
		}
		if (JadeitePanel && !JadeitePanel->IsInViewport())
		{
			JadeitePanel->AddToViewport(20);
		}
	}
}

void UKodFrontEndWidget::Travel(const FKodSkirmishSetup& TravelSetup)
{
	FKodLobbyMap LobbyMap;
	FKodSkirmishSetup UseSetup = TravelSetup;
	if (!UseSetup.FindMap(LobbyMap) || !LobbyMap.bPlayable)
	{
		UE_LOG(LogTemp, Warning, TEXT("KodLobby Travel refused Map=%s"), *UseSetup.MapId.ToString());
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("KodLobby Travel %s %s"), *LobbyMap.PackagePath, *UseSetup.ToOptions());
	UGameplayStatics::OpenLevel(this, FName(*LobbyMap.PackagePath), true, UseSetup.ToOptions());
}

void UKodFrontEndWidget::CycleMap()
{
	if (Maps.Num() == 0)
	{
		return;
	}
	for (int32 Step = 0; Step < Maps.Num(); ++Step)
	{
		MapCursor = (MapCursor + 1) % Maps.Num();
		if (Maps[MapCursor].bPlayable)
		{
			break;
		}
	}
	Setup.MapId = Maps[MapCursor].Id;
	Refresh();
}

void UKodFrontEndWidget::CycleDifficulty()
{
	if (Setup.Difficulty == EKodLobbyDifficulty::Easy)
	{
		Setup.Difficulty = EKodLobbyDifficulty::Normal;
	}
	else if (Setup.Difficulty == EKodLobbyDifficulty::Normal)
	{
		Setup.Difficulty = EKodLobbyDifficulty::Hard;
	}
	else
	{
		Setup.Difficulty = EKodLobbyDifficulty::Easy;
	}
	Refresh();
}

void UKodFrontEndWidget::CyclePlayerFaction()
{
	if (Setup.PlayerFaction == EKodRosterFaction::Vanguard)
	{
		Setup.PlayerFaction = EKodRosterFaction::Scinder;
	}
	else if (Setup.PlayerFaction == EKodRosterFaction::Scinder)
	{
		Setup.PlayerFaction = EKodRosterFaction::Random;
		Setup.Seed = FMath::RandRange(1, 100000);
	}
	else
	{
		Setup.PlayerFaction = EKodRosterFaction::Vanguard;
	}
	Refresh();
}

void UKodFrontEndWidget::CycleAiFaction()
{
	if (Setup.AiFaction == EKodRosterFaction::Vanguard)
	{
		Setup.AiFaction = EKodRosterFaction::Scinder;
	}
	else if (Setup.AiFaction == EKodRosterFaction::Scinder)
	{
		Setup.AiFaction = EKodRosterFaction::Random;
	}
	else
	{
		Setup.AiFaction = EKodRosterFaction::Vanguard;
	}
	Refresh();
}

void UKodFrontEndWidget::CycleColor(bool bPlayer)
{
	if (bPlayer)
	{
		Setup.PlayerColor = (Setup.PlayerColor + 1) % 6;
	}
	else
	{
		Setup.AiColor = (Setup.AiColor + 1) % 6;
	}
	Refresh();
}

void UKodFrontEndWidget::ToggleStart()
{
	Setup.PlayerStart = Setup.PlayerStart == 0 ? 1 : 0;
	Setup.AiStart = Setup.PlayerStart == 0 ? 1 : 0;
	Refresh();
}
