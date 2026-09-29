#pragma once

#include "KodUIScreenRoot.h"
#include "Style/KodUITypes.h"
#include "KodLobbyVersusWidget.generated.h"

class UCommonTextBlock;
class UHorizontalBox;
class UImage;
class UKodFactionBoardWidget;
class UKodIdentityStripWidget;
class UKodLabeledButton;
class UKodLadderRowWidget;
class UKodLobbyChatRailWidget;
class UVerticalBox;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKodLobbyClicked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodVersusFactionChanged, EKodVersusFaction, Faction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FKodVersusModeChanged, EKodVersusMode, Mode);

/**
 * Versus lobby root. Cyan glass. Not Ironstock.
 * Faction row is USA · RSF only — no RU, CN, or RANDOM slots.
 * Chat rail is the rightmost column (stamp v2.6).
 * Orange is reserved for PLAY RANKED and the local ladder row.
 * Parent WBP: /Game/UI/Lobby/WBP_LobbyVersus.
 */
UCLASS(Abstract, Blueprintable)
class KODUI_API UKodLobbyVersusWidget : public UKodUIScreenRoot
{
	GENERATED_BODY()

public:
	UKodLobbyVersusWidget();

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Lobby")
	static EKodLobbyChatRail GetRequiredChatRail();

	/** Always USA then RSF. The array length is the faction lock. */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Lobby")
	static void GetVersusFactions(TArray<EKodVersusFaction>& OutFactions);

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Lobby")
	static bool IsPlayableVersusFactionId(FName FactionId);

	/** True for RU, CN, RANDOM, and every id other than USA or RSF. */
	UFUNCTION(BlueprintPure, Category = "Kod|UI|Lobby")
	static bool IsRejectedVersusFactionId(FName FactionId);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	bool TrySetVersusFaction(EKodVersusFaction Faction);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	bool TrySetVersusFactionById(FName FactionId);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void SetVersusMode(EKodVersusMode Mode);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void SetSeasonStrip(const FText& InSeason, const FText& InRankName, const FText& InRecord);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void SetReadyStatus(const FText& InStatus);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void SetMapName(const FText& InMapName);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void SetMatchmakingStatus(const FText& InStatus);

	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void RebuildLadder(const TArray<FKodLadderRow>& Rows);

	/** Fictional rows from the v2.6 stamp, for Editor preview only. */
	UFUNCTION(BlueprintCallable, Category = "Kod|UI|Lobby")
	void GetStampPreviewRows(TArray<FKodLadderRow>& OutRows) const;

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Lobby")
	EKodVersusFaction GetSelectedFaction() const { return SelectedFaction; }

	UFUNCTION(BlueprintPure, Category = "Kod|UI|Lobby")
	EKodVersusMode GetSelectedMode() const { return SelectedMode; }

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodVersusFactionChanged OnVersusFactionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodVersusModeChanged OnVersusModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodLobbyClicked OnPlayRanked;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodLobbyClicked OnUnranked;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodLobbyClicked OnMaps;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodLobbyClicked OnFriends;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodLobbyClicked OnHelp;

	UPROPERTY(BlueprintAssignable, Category = "Kod|UI|Lobby")
	FKodLobbyClicked OnMenu;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativePreConstruct() override;

	void ApplyStampCopy();
	void ApplyFactionSelection();
	void ApplyModeSelection();
	void ApplyChatRailLock();
	void ApplyVariableCopy();

	UFUNCTION()
	void HandleFactionChosen(EKodVersusFaction Faction);
	UFUNCTION()
	void HandleModeTraining(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleModeOneVOne(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleModeTeams(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleModeTournaments(UKodLabeledButton* Button);
	UFUNCTION()
	void HandlePlayRanked(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleUnranked(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleMaps(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleFriends(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleHelp(UKodLabeledButton* Button);
	UFUNCTION()
	void HandleMenu(UKodLabeledButton* Button);

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Lobby")
	EKodVersusFaction SelectedFaction = EKodVersusFaction::USA;

	UPROPERTY(BlueprintReadOnly, Category = "Kod|UI|Lobby")
	EKodVersusMode SelectedMode = EKodVersusMode::OneVOne;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Kod|UI|Lobby")
	TSubclassOf<UKodLadderRowWidget> LadderRowClass;

	FText SeasonText;
	FText RankNameText;
	FText RecordText;
	FText ReadyText;
	FText MapText;
	FText MatchmakingText;
	FText LeagueText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UImage> HeroArt;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Lobby")
	TObjectPtr<UImage> GlassPlate;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> TitleText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> VersionBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Nav_Campaign;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Nav_Coop;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Nav_Versus;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Nav_Custom;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Nav_Collection;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Nav_Replays;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Mode_Training;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Mode_OneVOne;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Mode_Teams;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Mode_Tournaments;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UHorizontalBox> LobbyColumns;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UWidget> Column_Left;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UWidget> Column_Center;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UWidget> Column_Ladder;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLobbyChatRailWidget> ChatRail;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodIdentityStripWidget> Identity;

	/** USA and RSF tiles only. Cyan glass. Lives in the left column. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodFactionBoardWidget> FactionBoard;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> SeasonTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> RankName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> RecordLine;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Button_PlayRanked;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> ReadyStatus;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Button_Unranked;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Button_Maps;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Button_MapChip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> LadderHeader;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UVerticalBox> LadderList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> LadderPreview;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Util_Friends;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Util_Help;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UKodLabeledButton> Util_Menu;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget), Category = "Kod|UI|Lobby")
	TObjectPtr<UCommonTextBlock> MatchmakingStatus;
};
