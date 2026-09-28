#pragma once

#include "Style/KodUITypes.h"

/**
 * Layout stamp v4 proportions and content paths.
 * Widths are integer weights that sum to 100 with the trailing gutter.
 * Do not shrink the selection column to "make room" — the gutter is the remainder.
 */
namespace KodUILayout
{
	inline constexpr int32 HudColumnWeightMinimap = 18;
	inline constexpr int32 HudColumnWeightSelection = 42;
	inline constexpr int32 HudColumnWeightPortrait = 15;
	inline constexpr int32 HudColumnWeightCommand = 20;
	inline constexpr int32 HudColumnWeightGutter = 5;

	inline constexpr float HudBandHeightFraction = 0.28f;
	inline constexpr float GameplayViewportHeightFraction = 0.72f;

	inline constexpr int32 SelectionColumns = 8;
	inline constexpr int32 SelectionRows = 3;
	inline constexpr int32 SelectionCellCount = SelectionColumns * SelectionRows;
	inline constexpr int32 ControlGroupCount = 10;

	inline constexpr int32 CommandColumns = 5;
	inline constexpr int32 CommandRows = 3;
	inline constexpr int32 CommandCellCount = CommandColumns * CommandRows;

	/** Map-tool hit targets stay readable on dark metal. */
	inline constexpr float MapToolMinSizePx = 24.f;
	inline constexpr float MapToolPreferredSizePx = 32.f;
	inline constexpr float ClockMinWidthPx = 72.f;

	inline constexpr int32 VersusFactionCount = 2;

	inline constexpr int32 StampPreviewCredits = 1250;
	inline constexpr int32 StampPreviewOil = 680;
	inline constexpr int32 StampPreviewSupply = 48;
	inline constexpr int32 StampPreviewSupplyMax = 60;
	inline constexpr int32 StampPreviewClockHours = 11;
	inline constexpr int32 StampPreviewClockMinutes = 14;
	inline constexpr int32 StampPreviewHp = 75;
	inline constexpr int32 StampPreviewHpMax = 75;

	/** v1.1 portrait pin. HP is this tall and sits on the bottom edge of the art box. */
	inline constexpr float PortraitHpBarHeightPx = 8.f;
	/** Fixed ENERGY slot under the art box. Hiding the fill does not change this height. */
	inline constexpr float PortraitEnergySlotHeightPx = 18.f;
	inline constexpr float PortraitHpValueInsetPx = 4.f;
	inline constexpr float PortraitCallsignInsetPx = 4.f;

	inline constexpr const TCHAR* WBP_KodHUD = TEXT("/Game/UI/HUD/WBP_KodHUD");
	inline constexpr const TCHAR* WBP_ResourceBar = TEXT("/Game/UI/HUD/WBP_ResourceBar");
	inline constexpr const TCHAR* WBP_Minimap = TEXT("/Game/UI/HUD/WBP_Minimap");
	inline constexpr const TCHAR* WBP_SelectionPanel = TEXT("/Game/UI/HUD/WBP_SelectionPanel");
	inline constexpr const TCHAR* WBP_SelectionCell = TEXT("/Game/UI/HUD/WBP_SelectionCell");
	inline constexpr const TCHAR* WBP_PortraitFrame = TEXT("/Game/UI/HUD/WBP_PortraitFrame");
	inline constexpr const TCHAR* WBP_CommandCard = TEXT("/Game/UI/HUD/WBP_CommandCard");
	inline constexpr const TCHAR* WBP_CommandSlot = TEXT("/Game/UI/HUD/WBP_CommandSlot");
	inline constexpr const TCHAR* WBP_ChatMenuCluster = TEXT("/Game/UI/HUD/WBP_ChatMenuCluster");
	inline constexpr const TCHAR* WBP_LabeledButton = TEXT("/Game/UI/CommonUI/WBP_LabeledButton");
	inline constexpr const TCHAR* WBP_IdentityStrip = TEXT("/Game/UI/CommonUI/WBP_IdentityStrip");
	inline constexpr const TCHAR* WBP_MainMenu = TEXT("/Game/UI/MainMenu/WBP_MainMenu");
	inline constexpr const TCHAR* WBP_LobbyVersus = TEXT("/Game/UI/Lobby/WBP_LobbyVersus");
	inline constexpr const TCHAR* WBP_LobbyChatRail = TEXT("/Game/UI/Lobby/WBP_LobbyChatRail");
	inline constexpr const TCHAR* WBP_ChatMessageRow = TEXT("/Game/UI/Lobby/WBP_ChatMessageRow");
	inline constexpr const TCHAR* WBP_FactionBoard = TEXT("/Game/UI/Lobby/WBP_FactionBoard");
	inline constexpr const TCHAR* WBP_FactionTile = TEXT("/Game/UI/Lobby/WBP_FactionTile");
	inline constexpr const TCHAR* WBP_LadderRow = TEXT("/Game/UI/Lobby/WBP_LadderRow");

	inline int32 HudColumnWeight(EKodHudColumn Column)
	{
		switch (Column)
		{
		case EKodHudColumn::Minimap: return HudColumnWeightMinimap;
		case EKodHudColumn::Selection: return HudColumnWeightSelection;
		case EKodHudColumn::Portrait: return HudColumnWeightPortrait;
		case EKodHudColumn::Command: return HudColumnWeightCommand;
		case EKodHudColumn::Gutter: return HudColumnWeightGutter;
		default: return 0;
		}
	}

	/** Visual order on the selection panel is 1–9 then 0. Store index 0 is the last key. */
	inline int32 ControlGroupIndexFromVisual(int32 VisualIndex)
	{
		return (VisualIndex + 1) % ControlGroupCount;
	}
}

static_assert(
	KodUILayout::HudColumnWeightMinimap
		+ KodUILayout::HudColumnWeightSelection
		+ KodUILayout::HudColumnWeightPortrait
		+ KodUILayout::HudColumnWeightCommand
		+ KodUILayout::HudColumnWeightGutter
		== 100,
	"HUD column weights must sum to 100");
static_assert(KodUILayout::SelectionCellCount == 24, "Selection grid is 8x3");
static_assert(KodUILayout::CommandCellCount == 15, "Command card is 5x3");
static_assert(KodUILayout::VersusFactionCount == 2, "Versus row is USA and RSF only");
static_assert(static_cast<uint8>(EKodVersusFaction::USA) == 0, "USA is the first Versus faction");
static_assert(static_cast<uint8>(EKodVersusFaction::RSF) == 1, "RSF is the second Versus faction");
static_assert(static_cast<uint8>(EKodFactionAccentTheme::USA_Ironstock) == 0, "USA Ironstock is the HUD baseline");
static_assert(static_cast<uint8>(EKodFactionAccentTheme::RSF_RustOrange) == 1, "Accent ids are USA and RSF only; no RU or CN enumerator");
