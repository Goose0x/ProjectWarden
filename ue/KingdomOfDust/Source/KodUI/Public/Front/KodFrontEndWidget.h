#pragma once

#include "CommonActivatableWidget.h"
#include "Components/Button.h"
#include "Game/KodSkirmishSetup.h"
#include "KodFrontEndWidget.generated.h"

class UBorder;
class UHorizontalBox;
class UKodJadeiteTunerWidget;
class UVerticalBox;

enum class EKodFrontPage : uint8
{
	Menu,
	Faction,
	Lobby,
	Options
};

/**
 * CommonUI front end. Built in C++ so the menu does not need a widget asset.
 * Colors come from the HUD Ironstock tokens in UKodUIStyleLibrary.
 */
UCLASS()
class KODUI_API UKodFrontEndWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	virtual void NativeOnInitialized() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void Show(EKodFrontPage NextPage);
	void HandleAction(FName Action);

private:
	void BuildShell();
	void RebuildPage();
	void Refresh();
	void AddButton(UVerticalBox* Box, const FText& Label, FName Action, bool bEnabled, const FLinearColor* Fill = nullptr);
	void AddLine(UVerticalBox* Box, const FText& Line, bool bDim);
	void AddPortrait(UHorizontalBox* Row, const FText& Title, const FText& Mark, FName Action, FLinearColor Fill, bool bEnabled, bool bSelected);
	void Travel(const FKodSkirmishSetup& TravelSetup);
	void CycleMap();
	void CycleDifficulty();
	void CyclePlayerFaction();
	void CycleAiFaction();
	void CycleColor(bool bPlayer);
	void ToggleStart();
	FString MapLabel() const;

	EKodFrontPage Page = EKodFrontPage::Menu;
	bool bCampaignPick = false;
	bool bDeferUi = false;
	bool bRefreshQueued = false;
	int32 MapCursor = 0;
	FKodSkirmishSetup Setup;
	TArray<FKodLobbyMap> Maps;

	UPROPERTY()
	TObjectPtr<UBorder> Plate;

	UPROPERTY()
	TObjectPtr<UVerticalBox> Body;

	UPROPERTY()
	TObjectPtr<UKodJadeiteTunerWidget> JadeitePanel;
};

/** Click target that carries a menu action. UButton clicks are dynamic delegates, so they cannot bind a lambda. */
UCLASS()
class KODUI_API UKodFrontButton : public UButton
{
	GENERATED_BODY()

public:
	void BindMenu(UKodFrontEndWidget* MenuWidget, FName InAction);

private:
	UFUNCTION()
	void ForwardClick();

	UPROPERTY()
	TObjectPtr<UKodFrontEndWidget> MenuOwner;

	UPROPERTY()
	FName MenuAction;
};
