#include "KodLobbyVersusWidget.h"
#include "KodUI.h"
#include "KodIdentityStripWidget.h"
#include "KodLabeledButton.h"
#include "KodLadderRowWidget.h"
#include "KodLobbyChatRailWidget.h"
#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Style/KodUILayout.h"
#include "Style/KodUIStyle.h"

namespace KodLobbyPrivate
{
	void Tint(UKodLabeledButton* Button, const FLinearColor& Color)
	{
		if (Button)
		{
			Button->SetColorAndOpacity(Color);
		}
	}

	void PrepareFaction(UKodLabeledButton* Button)
	{
		if (!Button)
		{
			return;
		}
		Button->SetIsSelectable(true);
		Button->SetIsToggleable(false);
	}
}

UKodLobbyVersusWidget::UKodLobbyVersusWidget()
{
	Screen = EKodUIScreen::LobbyVersus;
	MaterialLanguage = EKodUIMaterialLanguage::CyanGlass;
	SeasonText = NSLOCTEXT("KodUI", "Season3", "SEASON 3");
	RankNameText = NSLOCTEXT("KodUI", "DiamondRank", "DIAMOND RANK");
	RecordText = NSLOCTEXT("KodUI", "StampRecord", "Wins: 128 · Losses: 47");
	ReadyText = NSLOCTEXT("KodUI", "ReadyStatus", "READY 1/1");
	MapText = NSLOCTEXT("KodUI", "DesertRail", "DESERT RAIL");
	MatchmakingText = NSLOCTEXT("KodUI", "MatchmakingActive", "MATCHMAKING ACTIVE");
	LeagueText = NSLOCTEXT("KodUI", "DiamondLeague", "Diamond");
}

EKodLobbyChatRail UKodLobbyVersusWidget::GetRequiredChatRail()
{
	return EKodLobbyChatRail::Right;
}

void UKodLobbyVersusWidget::GetVersusFactions(TArray<EKodVersusFaction>& OutFactions)
{
	OutFactions.Reset();
	OutFactions.Add(EKodVersusFaction::USA);
	OutFactions.Add(EKodVersusFaction::RSF);
	check(OutFactions.Num() == KodUILayout::VersusFactionCount);
}

bool UKodLobbyVersusWidget::IsPlayableVersusFactionId(FName FactionId)
{
	return FactionId == FName(TEXT("USA")) || FactionId == FName(TEXT("RSF"));
}

bool UKodLobbyVersusWidget::IsRejectedVersusFactionId(FName FactionId)
{
	const FString Id = FactionId.ToString();
	// Director lock. These ids stay off the Versus row — they are not dimmed ghost slots.
	const TCHAR* LockedOut[] = { TEXT("RU"), TEXT("CN"), TEXT("RANDOM") };
	for (const TCHAR* Name : LockedOut)
	{
		if (Id.Equals(Name, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}
	return !IsPlayableVersusFactionId(FactionId);
}

bool UKodLobbyVersusWidget::TrySetVersusFaction(EKodVersusFaction Faction)
{
	const bool bAllowed = Faction == EKodVersusFaction::USA || Faction == EKodVersusFaction::RSF;
	if (!bAllowed)
	{
		UE_LOG(LogKodUI, Error, TEXT("Versus faction row is USA and RSF only."));
		return false;
	}
	SelectedFaction = Faction;
	ApplyFactionSelection();
	OnVersusFactionChanged.Broadcast(Faction);
	return true;
}

bool UKodLobbyVersusWidget::TrySetVersusFactionById(FName FactionId)
{
	if (!IsPlayableVersusFactionId(FactionId))
	{
		UE_LOG(LogKodUI, Error, TEXT("Rejected Versus faction '%s'. Legal ids are USA and RSF."), *FactionId.ToString());
		return false;
	}
	const EKodVersusFaction Faction = FactionId == FName(TEXT("RSF"))
		? EKodVersusFaction::RSF
		: EKodVersusFaction::USA;
	return TrySetVersusFaction(Faction);
}

void UKodLobbyVersusWidget::SetVersusMode(EKodVersusMode Mode)
{
	SelectedMode = Mode;
	ApplyModeSelection();
	OnVersusModeChanged.Broadcast(Mode);
}

void UKodLobbyVersusWidget::SetSeasonStrip(const FText& InSeason, const FText& InRankName, const FText& InRecord)
{
	SeasonText = InSeason;
	RankNameText = InRankName;
	RecordText = InRecord;
	ApplyVariableCopy();
}

void UKodLobbyVersusWidget::SetReadyStatus(const FText& InStatus)
{
	ReadyText = InStatus;
	ApplyVariableCopy();
}

void UKodLobbyVersusWidget::SetMapName(const FText& InMapName)
{
	MapText = InMapName;
	ApplyVariableCopy();
}

void UKodLobbyVersusWidget::SetMatchmakingStatus(const FText& InStatus)
{
	MatchmakingText = InStatus;
	ApplyVariableCopy();
}

void UKodLobbyVersusWidget::GetStampPreviewRows(TArray<FKodLadderRow>& OutRows) const
{
	OutRows.Reset();
	auto Add = [&OutRows](int32 Rank, const TCHAR* Name, int32 Mmr, int32 Wins, int32 Losses, bool bLocal)
	{
		FKodLadderRow Row;
		Row.Rank = Rank;
		Row.Name = FText::AsCultureInvariant(FString(Name));
		Row.Mmr = Mmr;
		Row.Wins = Wins;
		Row.Losses = Losses;
		Row.bLocalPlayer = bLocal;
		OutRows.Add(Row);
	};
	// Order matches the v2.6 stamp frame, including the highlighted local row.
	Add(1, TEXT("Warden Prime"), 2457, 162, 38, false);
	Add(2, TEXT("Capt. Sokolov"), 2321, 138, 52, false);
	Add(3, TEXT("Lt. Zhang"), 2218, 119, 61, false);
	Add(4, TEXT("Operative Nyx"), 1764, 87, 81, false);
	Add(5, TEXT("Commander Doe [WARD]"), 1987, 128, 47, true);
	Add(6, TEXT("Col. Beshirov"), 1652, 76, 90, false);
	Add(7, TEXT("Agent Kuro"), 1543, 65, 103, false);
	Add(8, TEXT("Spec. Talon"), 1432, 54, 116, false);
	Add(9, TEXT("Vik Kommand"), 1876, 98, 72, false);
	Add(10, TEXT("Maj. Reinhardt"), 2103, 104, 68, false);
}

void UKodLobbyVersusWidget::RebuildLadder(const TArray<FKodLadderRow>& Rows)
{
	if (LadderRowClass && LadderList)
	{
		LadderList->ClearChildren();
		for (const FKodLadderRow& Row : Rows)
		{
			if (UKodLadderRowWidget* Widget = CreateWidget<UKodLadderRowWidget>(this, LadderRowClass))
			{
				LadderList->AddChild(Widget);
				Widget->ApplyRow(Row);
			}
		}
		if (LadderPreview)
		{
			LadderPreview->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	if (!LadderPreview)
	{
		return;
	}
	FString Preview;
	for (const FKodLadderRow& Row : Rows)
	{
		if (!Preview.IsEmpty())
		{
			Preview.Append(TEXT("\n"));
		}
		Preview.Appendf(TEXT("%d  %s  %d  %d  %d"), Row.Rank, *Row.Name.ToString(), Row.Mmr, Row.Wins, Row.Losses);
	}
	LadderPreview->SetText(FText::FromString(Preview));
	LadderPreview->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UKodLobbyVersusWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	using namespace KodLobbyPrivate;
	PrepareFaction(Faction_USA);
	PrepareFaction(Faction_RSF);
	PrepareFaction(Mode_Training);
	PrepareFaction(Mode_OneVOne);
	PrepareFaction(Mode_Teams);
	PrepareFaction(Mode_Tournaments);

	if (Faction_USA) { Faction_USA->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleFactionUSA); }
	if (Faction_RSF) { Faction_RSF->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleFactionRSF); }
	if (Mode_Training) { Mode_Training->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleModeTraining); }
	if (Mode_OneVOne) { Mode_OneVOne->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleModeOneVOne); }
	if (Mode_Teams) { Mode_Teams->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleModeTeams); }
	if (Mode_Tournaments) { Mode_Tournaments->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleModeTournaments); }
	if (Button_PlayRanked) { Button_PlayRanked->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandlePlayRanked); }
	if (Button_Unranked) { Button_Unranked->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleUnranked); }
	if (Button_Maps) { Button_Maps->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleMaps); }
	if (Button_MapChip) { Button_MapChip->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleMaps); }
	if (Util_Friends) { Util_Friends->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleFriends); }
	if (Util_Help) { Util_Help->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleHelp); }
	if (Util_Menu) { Util_Menu->OnLabeledClicked.AddUniqueDynamic(this, &UKodLobbyVersusWidget::HandleMenu); }
}

void UKodLobbyVersusWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyStampCopy();
	ApplyChatRailLock();
	if (IsDesignTime() && LadderPreview)
	{
		TArray<FKodLadderRow> PreviewRows;
		GetStampPreviewRows(PreviewRows);
		FString Preview;
		for (const FKodLadderRow& Row : PreviewRows)
		{
			if (!Preview.IsEmpty())
			{
				Preview.Append(TEXT("\n"));
			}
			Preview.Appendf(TEXT("%d  %s  %d  %d  %d"), Row.Rank, *Row.Name.ToString(), Row.Mmr, Row.Wins, Row.Losses);
		}
		LadderPreview->SetText(FText::FromString(Preview));
		LadderPreview->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UKodLobbyVersusWidget::ApplyStampCopy()
{
	const FLinearColor Cyan = UKodUIStyleLibrary::GetCyanActive();
	const FLinearColor Dim(Cyan.R, Cyan.G, Cyan.B, 0.4f);
	const FSlateColor Text(UKodUIStyleLibrary::GetWhiteText());

	if (HeroArt)
	{
		HeroArt->SetColorAndOpacity(FLinearColor::White);
	}
	if (GlassPlate)
	{
		GlassPlate->SetColorAndOpacity(UKodUIStyleLibrary::GetPanelBlue());
	}
	if (TitleText)
	{
		TitleText->SetText(NSLOCTEXT("KodUI", "WardenVersus", "Warden Versus"));
		TitleText->SetColorAndOpacity(Text);
	}
	if (VersionBadge)
	{
		VersionBadge->SetText(NSLOCTEXT("KodUI", "LobbyBadge", "v2.6"));
		VersionBadge->SetColorAndOpacity(FSlateColor(Cyan));
	}

	if (Nav_Campaign) { Nav_Campaign->SetStampLabel(NSLOCTEXT("KodUI", "LobbyCampaign", "CAMPAIGN")); }
	if (Nav_Coop) { Nav_Coop->SetStampLabel(NSLOCTEXT("KodUI", "LobbyCoop", "CO-OP")); }
	if (Nav_Versus) { Nav_Versus->SetStampLabel(NSLOCTEXT("KodUI", "LobbyVersus", "VERSUS")); }
	if (Nav_Custom) { Nav_Custom->SetStampLabel(NSLOCTEXT("KodUI", "LobbyCustom", "CUSTOM")); }
	if (Nav_Collection) { Nav_Collection->SetStampLabel(NSLOCTEXT("KodUI", "LobbyCollection", "COLLECTION")); }
	if (Nav_Replays)
	{
		Nav_Replays->SetStampLabel(NSLOCTEXT("KodUI", "LobbyReplays", "REPLAYS — UNSHIPPED"));
		Nav_Replays->SetIsEnabled(false);
	}
	KodLobbyPrivate::Tint(Nav_Campaign, Dim);
	KodLobbyPrivate::Tint(Nav_Coop, Dim);
	KodLobbyPrivate::Tint(Nav_Versus, Cyan);
	KodLobbyPrivate::Tint(Nav_Custom, Dim);
	KodLobbyPrivate::Tint(Nav_Collection, Dim);
	KodLobbyPrivate::Tint(Nav_Replays, Dim);

	if (Mode_Training) { Mode_Training->SetStampLabel(NSLOCTEXT("KodUI", "ModeTraining", "TRAINING")); }
	if (Mode_OneVOne) { Mode_OneVOne->SetStampLabel(NSLOCTEXT("KodUI", "Mode1v1", "1V1")); }
	if (Mode_Teams) { Mode_Teams->SetStampLabel(NSLOCTEXT("KodUI", "ModeTeams", "TEAMS")); }
	if (Mode_Tournaments) { Mode_Tournaments->SetStampLabel(NSLOCTEXT("KodUI", "ModeTournaments", "TOURNAMENTS")); }

	if (Faction_USA) { Faction_USA->SetStampLabel(NSLOCTEXT("KodUI", "FactionUSA", "USA")); }
	if (Faction_RSF) { Faction_RSF->SetStampLabel(NSLOCTEXT("KodUI", "FactionRSF", "RSF")); }

	if (Button_PlayRanked)
	{
		Button_PlayRanked->SetStampLabel(NSLOCTEXT("KodUI", "PlayRanked", "PLAY RANKED"));
		Button_PlayRanked->SetColorAndOpacity(UKodUIStyleLibrary::GetCommitColor(GetMaterialLanguage()));
	}
	if (Button_Unranked) { Button_Unranked->SetStampLabel(NSLOCTEXT("KodUI", "Unranked", "UNRANKED")); }
	if (Button_Maps) { Button_Maps->SetStampLabel(NSLOCTEXT("KodUI", "Maps", "MAPS")); }
	KodLobbyPrivate::Tint(Button_Unranked, Cyan);
	KodLobbyPrivate::Tint(Button_Maps, Cyan);
	KodLobbyPrivate::Tint(Button_MapChip, Cyan);

	if (LadderHeader)
	{
		LadderHeader->SetText(NSLOCTEXT("KodUI", "LadderHeader", "RANK    NAME    MMR    W    L"));
		LadderHeader->SetColorAndOpacity(FSlateColor(Cyan));
	}

	if (Util_Friends)
	{
		Util_Friends->SetShowLabel(false);
		Util_Friends->SetStampLabel(NSLOCTEXT("KodUI", "LobbyFriends", "FRIENDS"));
	}
	if (Util_Help)
	{
		Util_Help->SetShowLabel(false);
		Util_Help->SetStampLabel(NSLOCTEXT("KodUI", "LobbyHelp", "HELP"));
	}
	if (Util_Menu)
	{
		Util_Menu->SetShowLabel(false);
		Util_Menu->SetStampLabel(NSLOCTEXT("KodUI", "LobbyMenu", "MENU"));
	}
	KodLobbyPrivate::Tint(Util_Friends, Cyan);
	KodLobbyPrivate::Tint(Util_Help, Cyan);
	KodLobbyPrivate::Tint(Util_Menu, Cyan);

	if (Identity)
	{
		Identity->SetLeagueLine(LeagueText);
	}

	ApplyVariableCopy();
	ApplyFactionSelection();
	ApplyModeSelection();
}

void UKodLobbyVersusWidget::ApplyVariableCopy()
{
	const FSlateColor Text(UKodUIStyleLibrary::GetWhiteText());
	const FLinearColor Cyan = UKodUIStyleLibrary::GetCyanActive();
	if (SeasonTitle)
	{
		SeasonTitle->SetText(SeasonText);
		SeasonTitle->SetColorAndOpacity(FSlateColor(Cyan));
	}
	if (RankName)
	{
		RankName->SetText(RankNameText);
		RankName->SetColorAndOpacity(Text);
	}
	if (RecordLine)
	{
		RecordLine->SetText(RecordText);
		RecordLine->SetColorAndOpacity(Text);
	}
	if (ReadyStatus)
	{
		ReadyStatus->SetText(ReadyText);
		ReadyStatus->SetColorAndOpacity(FSlateColor(Cyan));
	}
	if (Button_MapChip)
	{
		Button_MapChip->SetStampLabel(MapText);
	}
	if (MatchmakingStatus)
	{
		MatchmakingStatus->SetText(MatchmakingText);
		MatchmakingStatus->SetColorAndOpacity(FSlateColor(FLinearColor(Cyan.R, Cyan.G, Cyan.B, 0.55f)));
	}
}

void UKodLobbyVersusWidget::ApplyFactionSelection()
{
	const FLinearColor Cyan = UKodUIStyleLibrary::GetCyanActive();
	const FLinearColor Dim(Cyan.R, Cyan.G, Cyan.B, 0.45f);
	if (Faction_USA)
	{
		KodLobbyPrivate::PrepareFaction(Faction_USA);
		Faction_USA->SetIsSelected(SelectedFaction == EKodVersusFaction::USA);
		Faction_USA->SetColorAndOpacity(SelectedFaction == EKodVersusFaction::USA ? Cyan : Dim);
	}
	if (Faction_RSF)
	{
		KodLobbyPrivate::PrepareFaction(Faction_RSF);
		Faction_RSF->SetIsSelected(SelectedFaction == EKodVersusFaction::RSF);
		Faction_RSF->SetColorAndOpacity(SelectedFaction == EKodVersusFaction::RSF ? Cyan : Dim);
	}
}

void UKodLobbyVersusWidget::ApplyModeSelection()
{
	const FLinearColor Cyan = UKodUIStyleLibrary::GetCyanActive();
	const FLinearColor Dim(Cyan.R, Cyan.G, Cyan.B, 0.45f);
	auto Paint = [&](UKodLabeledButton* Button, EKodVersusMode Mode)
	{
		if (!Button)
		{
			return;
		}
		KodLobbyPrivate::PrepareFaction(Button);
		const bool bSelected = SelectedMode == Mode;
		Button->SetIsSelected(bSelected);
		Button->SetColorAndOpacity(bSelected ? Cyan : Dim);
	};
	Paint(Mode_Training, EKodVersusMode::Training);
	Paint(Mode_OneVOne, EKodVersusMode::OneVOne);
	Paint(Mode_Teams, EKodVersusMode::Teams);
	Paint(Mode_Tournaments, EKodVersusMode::Tournaments);
}

void UKodLobbyVersusWidget::ApplyChatRailLock()
{
	if (UKodLobbyChatRailWidget::GetRequiredRail() != EKodLobbyChatRail::Right)
	{
		UE_LOG(LogKodUI, Error, TEXT("Lobby chat rail lock was broken. v2.6 requires the right rail."));
	}
	if (!LobbyColumns || !ChatRail)
	{
		return;
	}

	const int32 ChatIndex = LobbyColumns->GetChildIndex(ChatRail);
	if (ChatIndex == INDEX_NONE)
	{
		UE_LOG(LogKodUI, Error, TEXT("ChatRail must be a direct child of LobbyColumns, as the rightmost column."));
		return;
	}

	const int32 Last = LobbyColumns->GetChildrenCount() - 1;
	if (ChatIndex != Last)
	{
		UE_LOG(LogKodUI, Warning, TEXT("Moving ChatRail to the rightmost LobbyColumns slot (stamp v2.6)."));
		LobbyColumns->RemoveChild(ChatRail);
		LobbyColumns->AddChild(ChatRail);
	}

	const int32 LeftIndex = LobbyColumns->GetChildIndex(Column_Left);
	const int32 RailIndex = LobbyColumns->GetChildIndex(ChatRail);
	if (LeftIndex != INDEX_NONE && RailIndex != INDEX_NONE && LeftIndex > RailIndex)
	{
		UE_LOG(LogKodUI, Error, TEXT("Column_Left must stay left of ChatRail. Lower-left is identity and USA/RSF."));
	}
}

void UKodLobbyVersusWidget::HandleFactionUSA(UKodLabeledButton* Button)
{
	(void)Button;
	TrySetVersusFaction(EKodVersusFaction::USA);
}

void UKodLobbyVersusWidget::HandleFactionRSF(UKodLabeledButton* Button)
{
	(void)Button;
	TrySetVersusFaction(EKodVersusFaction::RSF);
}

void UKodLobbyVersusWidget::HandleModeTraining(UKodLabeledButton* Button)
{
	(void)Button;
	SetVersusMode(EKodVersusMode::Training);
}

void UKodLobbyVersusWidget::HandleModeOneVOne(UKodLabeledButton* Button)
{
	(void)Button;
	SetVersusMode(EKodVersusMode::OneVOne);
}

void UKodLobbyVersusWidget::HandleModeTeams(UKodLabeledButton* Button)
{
	(void)Button;
	SetVersusMode(EKodVersusMode::Teams);
}

void UKodLobbyVersusWidget::HandleModeTournaments(UKodLabeledButton* Button)
{
	(void)Button;
	SetVersusMode(EKodVersusMode::Tournaments);
}

void UKodLobbyVersusWidget::HandlePlayRanked(UKodLabeledButton* Button)
{
	(void)Button;
	OnPlayRanked.Broadcast();
}

void UKodLobbyVersusWidget::HandleUnranked(UKodLabeledButton* Button)
{
	(void)Button;
	OnUnranked.Broadcast();
}

void UKodLobbyVersusWidget::HandleMaps(UKodLabeledButton* Button)
{
	(void)Button;
	OnMaps.Broadcast();
}

void UKodLobbyVersusWidget::HandleFriends(UKodLabeledButton* Button)
{
	(void)Button;
	OnFriends.Broadcast();
}

void UKodLobbyVersusWidget::HandleHelp(UKodLabeledButton* Button)
{
	(void)Button;
	OnHelp.Broadcast();
}

void UKodLobbyVersusWidget::HandleMenu(UKodLabeledButton* Button)
{
	(void)Button;
	OnMenu.Broadcast();
}
