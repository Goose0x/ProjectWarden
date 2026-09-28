# Warden UI — Unreal construction checklist

QA-stamped screens are implemented as C++ widget shells in `KodUI`. The Editor creates the Widget Blueprints and materials. **Do not commit `.uasset` or `.umap` files** for these shells — not placeholders, not empty assets, not exported fakes.

`AKodHUD` still owns the in-game widget. It does not link `KodUI` (that would cycle through `KodUI` → `KodCore`). Point `HudWidgetClass` at a Blueprint whose parent is `UKodHudRootWidget`.

## Material language

| Screen | Language | C++ root | Do not use |
|--------|----------|----------|------------|
| In-game HUD | **Ironstock** — amber on dark metal | `UKodHudRootWidget` | Cyan glass, orange commit tint |
| Main Menu | **Cyan glass** — frosted panes over hero art | `UKodMainMenuWidget` | Ironstock metal, Versus chrome |
| Lobby Versus | **Cyan glass** | `UKodLobbyVersusWidget` | Ironstock metal |

Tokens live on `UKodUIStyleLibrary`. `GetCommitColor` returns orange only for cyan glass. An Ironstock caller gets amber and an error log.

Stand-in tints are not the final materials. Replace them in the Editor with the paths below. Keep the same split: metal/amber in the match, cyan glass on the front end.

## Stamp sources

| Stamp | What it locks |
|-------|----------------|
| HUD layout v4 | Bottom band **28%** of viewport height. Columns **18 / 42 / 15 / 20** plus a trailing gutter **5**. Weights are `KodUILayout` and sum to 100. |
| HUD Ironstock paint v3 | Visual lock on that shell. Resources are icon + number (Credits, Oil, Supply). Map tools are idle worker, army, ping. |
| Main Menu glass paint v1 | One orange **PLAY VERSUS**. Secondary nav and briefing. No Versus chrome. |
| Lobby Versus glass v2.6 | Cyan glass. **USA · RSF only**. Chat on the **right** rail. Orange only on **PLAY RANKED** (and the local ladder row). Badge **v2.6**. |
| Versus chat component v1 | Right-rail internals: ChatRoot, ChannelTabs, MessageList, MessageRow, ChatInput. Cyan glass. Secondary to PLAY RANKED. |
| Faction boards USA · RSF v1 | `FactionBoard` with `FactionTile_USA` and `FactionTile_RSF` only. States Idle / Hover / Selected / Disabled. |
| Portrait panel pin v1.1 | **PASS.** `Img_Portrait` fills the art box. `Prog_Health` is flush on that art bottom, not a separate bay. `Slot_Energy` stays a fixed height under HP. One `Txt_Callsign`: Black Widow. |
| HUD accent USA | **HOLD** pending USA v4 QA PASS. `USA_TanGreenGold` is not an authoritative lock. Do not treat tan/patriotic v3 as final. Olive drab, gunmetal, and gold are not landed. |
| HUD accent RSF v3 | **PASS.** `RSF_MetalStoneOrangeArch` is sandstone/adobe, warm mortar, orange. Not gothic iron. No arch widget. |
| Minimap tool placement | **HOLD** pending the updated template plate QA PASS. Tools on the right of the minimap are not landed. The in-tree header strip is not the lock. Column weight stays 18. |
| HUD accent CN · RU v2 | **PASS / KEEP.** `CN_JadeStoneGoldRed` and `RU_SovietColdBlueIce` stay the prior v2 tables. Preview only. Not Versus tiles. |

## Content paths

Create these in the Editor. Parent class is the C++ type. Instance names inside a parent Blueprint must match the BindWidget column exactly.

| Widget Blueprint | Parent class | Path |
|------------------|--------------|------|
| `WBP_KodHUD` | `UKodHudRootWidget` | `/Game/UI/HUD/WBP_KodHUD` |
| `WBP_ResourceBar` | `UKodResourceBarWidget` | `/Game/UI/HUD/WBP_ResourceBar` |
| `WBP_Minimap` | `UKodMinimapWidget` | `/Game/UI/HUD/WBP_Minimap` |
| `WBP_SelectionPanel` | `UKodSelectionPanelWidget` | `/Game/UI/HUD/WBP_SelectionPanel` |
| `WBP_SelectionCell` | `UKodSelectionCellWidget` | `/Game/UI/HUD/WBP_SelectionCell` |
| `WBP_PortraitFrame` | `UKodPortraitFrameWidget` | `/Game/UI/HUD/WBP_PortraitFrame` |
| `WBP_CommandCard` | `UKodCommandCardWidget` | `/Game/UI/HUD/WBP_CommandCard` |
| `WBP_CommandSlot` | `UKodCommandSlotButton` | `/Game/UI/HUD/WBP_CommandSlot` |
| `WBP_ChatMenuCluster` | `UKodChatMenuClusterWidget` | `/Game/UI/HUD/WBP_ChatMenuCluster` |
| `WBP_LabeledButton` | `UKodLabeledButton` | `/Game/UI/CommonUI/WBP_LabeledButton` |
| `WBP_IdentityStrip` | `UKodIdentityStripWidget` | `/Game/UI/CommonUI/WBP_IdentityStrip` |
| `WBP_MainMenu` | `UKodMainMenuWidget` | `/Game/UI/MainMenu/WBP_MainMenu` |
| `WBP_LobbyVersus` | `UKodLobbyVersusWidget` | `/Game/UI/Lobby/WBP_LobbyVersus` |
| `WBP_LobbyChatRail` (`W_ChatRail`) | `UKodLobbyChatRailWidget` | `/Game/UI/Lobby/WBP_LobbyChatRail` |
| `WBP_ChatMessageRow` (`W_ChatMessageRow`) | `UKodChatMessageRowWidget` | `/Game/UI/Lobby/WBP_ChatMessageRow` |
| `WBP_FactionBoard` (`W_FactionBoard`) | `UKodFactionBoardWidget` | `/Game/UI/Lobby/WBP_FactionBoard` |
| `WBP_FactionTile` | `UKodFactionTileWidget` | `/Game/UI/Lobby/WBP_FactionTile` |
| `WBP_LadderRow` | `UKodLadderRowWidget` | `/Game/UI/Lobby/WBP_LadderRow` |

Constants for the same paths are in `KodUILayout`.

### Materials (Editor only)

| Path | Use |
|------|-----|
| `/Game/UI/Styles/Ironstock/` | HUD plates, rivets, amber readouts, HP green. No cyan glass. |
| `/Game/UI/Styles/CyanGlass/` | Front-end panes, cyan borders, orange commit for PLAY VERSUS and PLAY RANKED only. No riveted metal. |

Resource icon brushes on the HUD: a credits mark, an oil drop, a people mark. Not crystal / gas icons.

Hero art is a texture on `HeroArt` (hangar for the main menu, lobby backdrop for Versus). Do not ship a flat empty plate as the final look.

## HUD — `UKodHudRootWidget` (Ironstock)

`NativePreConstruct` anchors `HudBand` to the bottom 28% when it is a canvas slot, pins `ResourceBar` to the top-right, and sets horizontal fill weights 18 / 42 / 15 / 20 / 5.

| Region | Widget name | Type | Notes |
|--------|-------------|------|-------|
| Resource strip | `ResourceBar` | `UKodResourceBarWidget` | Top-right. Outside the bottom band. |
| Bottom band | `HudBand` | `UWidget` | Canvas child, lower 28%. |
| Column host | `HudColumns` | `UHorizontalBox` | Direct parent of the five columns. |
| Minimap column | `Column_Minimap` | `UWidget` | Weight 18. Holds `Minimap`. |
| Selection column | `Column_Selection` | `UWidget` | Weight 42. Do not steal this for the gutter. |
| Portrait column | `Column_Portrait` | `UWidget` | Weight 15. Holds `Portrait`. |
| Command column | `Column_Command` | `UWidget` | Weight 20. Chat/Menu above the card. |
| Gutter | `Column_Gutter` | `USpacer` | Weight 5. Trailing column. |
| Minimap cluster | `Minimap` | `UKodMinimapWidget` | |
| Selection | `SelectionPanel` | `UKodSelectionPanelWidget` | |
| Portrait | `Portrait` | `UKodPortraitFrameWidget` | |
| Command card | `CommandCard` | `UKodCommandCardWidget` | |
| Chat / Menu | `ChatMenuCluster` | `UKodChatMenuClusterWidget` | Above the card. |
| Optional plate | `IronstockPlate` | `UImage` | Metal tint. Optional. |

### Resource bar — `UKodResourceBarWidget`

Icon + number only. No CREDITS / OIL / SUPPLY captions.

| Name | Type |
|------|------|
| `Icon_Credits` | `UImage` |
| `Value_Credits` | `UCommonTextBlock` |
| `Icon_Oil` | `UImage` |
| `Value_Oil` | `UCommonTextBlock` |
| `Icon_Supply` | `UImage` |
| `Value_Supply` | `UCommonTextBlock` (`current/max`) |

`SetDustDisplay` still writes the Credits chip so the M1 Dust Crystal wallet has a place to land. `SetOil` and `SetSupply` are ready for later wallets. Designer preview shows 1250 / 680 / 48/60.

### Minimap — `UKodMinimapWidget`

**HOLD.** Map-tool placement is not locked. Director direction (not yet QA PASS) puts the three tools on the **right** of the minimap, with the map filling the bay flush to the box. Do not move the tools to that right edge until the updated template plate passes QA. Do not treat the header `ToolChipStrip` as the authoritative plate.

The current tree still has clock plus `ToolChipStrip` in the header and `MinimapImage` filling under it. That is unfinished placement, not a stamp. Column weight stays 18. Tools stay idle worker, army, ping. `MapToolChipSizePx` is 16. Optional size boxes receive that override.

```
Minimap (vertical bay, column weight 18)
  Header (auto height)
    ClockText
    ToolChipStrip          horizontal, same header row as the clock
      Tool_IdleWorker
      Tool_Army
      Tool_Ping
  MinimapImage             fill — owns the left bay
```

| Name | Type | Required |
|------|------|----------|
| `ClockText` | `UCommonTextBlock` | yes |
| `Size_Clock` | `USizeBox` | no |
| `ToolChipStrip` | `UHorizontalBox` | yes |
| `Tool_IdleWorker` | `UKodLabeledButton` | yes |
| `Tool_Army` | `UKodLabeledButton` | yes |
| `Tool_Ping` | `UKodLabeledButton` | yes |
| `Size_IdleWorker` / `Size_Army` / `Size_Ping` | `USizeBox` | no |
| `MinimapImage` | `UImage` | yes |

### Selection — `UKodSelectionPanelWidget`

No "CONTROL GROUPS" title. Group keys are `Group_1` … `Group_9`, `Group_0` (`UKodLabeledButton`). Store index 0 is the last key.

Cells are row-major, 8×3: `Cell_00` … `Cell_23` (`UKodSelectionCellWidget`). Each cell Blueprint names `Icon` and `HealthPip` (both optional inside the cell).

`OverflowTabRail` (`UWidget`, optional) is the page rail. It stays in the hierarchy and starts Hidden. It is not extra permanent cells.

### Portrait — `UKodPortraitFrameWidget` (`W_PortraitPanel`)

**PASS.** Geometry is authoritative from the v1.1 stamps (`hud-accent-usa-ironstock-v1.1` / `hud-accent-rsf-v1.1`). The accent color-language hold does not cover this tree. Do not bake sheet labels such as "Portrait 15%" into the widget.

```
W_PortraitPanel
  Art box (overlay or canvas)
    Img_Portrait     fill, margin 0, no letterbox
    Prog_Health      bottom of that same art box (integrated, not a row underneath)
    Txt_Callsign     one text widget, top of the art, "Black Widow"
    Txt_Health       optional numeral on the health bar
  Slot_Energy        fixed height, directly under the art box
    Prog_Energy      hide when unused; slot height stays
```

`ApplyPortraitGeometry` fills `Img_Portrait` edge-to-edge, docks `Prog_Health` to the art-box bottom, and sets `Slot_Energy` to `PortraitEnergySlotHeightPx`. A vertical-box row of HP under the image is logged as an error. `Prog_Energy` uses `Hidden` when unused. `Slot_Energy` is never `Collapsed`.

| Name | Type | Notes |
|------|------|-------|
| `Img_Portrait` | `UImage` | Art box fill. Brush margin 0. |
| `Prog_Health` | `UProgressBar` | Green. Flush to the art bottom. |
| `Txt_Health` | `UCommonTextBlock` | Optional. `current/max` on the health bar. Not a callsign. |
| `Txt_Callsign` | `UCommonTextBlock` | The only callsign. Exactly **Black Widow**. Do not add a second label. |
| `Slot_Energy` | `USizeBox` | Fixed height under HP. |
| `Prog_Energy` | `UProgressBar` | Child of `Slot_Energy`. Hidden when the unit has no energy. |

`Txt_Callsign` is the only widget set to Black Widow. QA's stacked double-print on the stamp is not copied.

### Command card — `UKodCommandCardWidget`

No "COMMAND CARD" title. 5×3, glyphs only. Top row tooltips are Move / Stop / Hold / Patrol / Attack. Those words are not the button face. Charge numerals use `SetChargeCount`; negative hides them.

| | | | | |
|--|--|--|--|--|
| `Slot_Q` | `Slot_W` | `Slot_E` | `Slot_R` | `Slot_T` |
| `Slot_A` | `Slot_S` | `Slot_D` | `Slot_F` | `Slot_G` |
| `Slot_Z` | `Slot_X` | `Slot_C` | `Slot_V` | `Slot_B` |

Each slot is `UKodCommandSlotButton` (`WBP_CommandSlot`) with optional text blocks `GlyphText` and `ChargeText`.

`CardPages` (`UWidgetSwitcher`, optional): index 0 is the grid, index 1 is the build submenu. `OpenBuildSubmenu` / `CloseBuildSubmenu` (Esc) swap them. Hold and Patrol are card ids (`Hold`, `Patrol`) and are not yet values on `EKodCommandType`.

### Chat / Menu cluster — `UKodChatMenuClusterWidget`

| Name | Required | Label |
|------|----------|-------|
| `Button_Chat` | yes | CHAT |
| `Button_Menu` | yes | MENU |
| `Button_Help` | no | ? |

Help is reserved by layout v4. Paint v3 may omit it.

## Main Menu — `UKodMainMenuWidget` (cyan glass)

No faction tiles, ladder, matchmaking, or PLAY RANKED. Orange is `Button_PlayVersus` only. `OnMainMenuAction` reports the click. TRAINING is the deep-link signal toward the Versus TRAINING sub-tab; this widget does not change levels by itself.

| Name | Type | Copy |
|------|------|------|
| `HeroArt` | `UImage` | Full-bleed hangar |
| `GlassPlate` | `UImage` | Optional frosted pane |
| `Identity` | `UKodIdentityStripWidget` | Avatar, commander, `[WARD]` |
| `Button_PlayVersus` | `UKodLabeledButton` | PLAY VERSUS |
| `Caption_VersusLobby` | `UCommonTextBlock` | Versus lobby |
| `Nav_Training` | `UKodLabeledButton` | TRAINING |
| `Nav_Campaign` | `UKodLabeledButton` | CAMPAIGN |
| `Nav_Coop` | `UKodLabeledButton` | CO-OP |
| `Nav_Armory` | `UKodLabeledButton` | ARMORY |
| `Nav_Options` | `UKodLabeledButton` | OPTIONS |
| `Nav_Exit` | `UKodLabeledButton` | EXIT |
| `Util_Friends` | `UKodLabeledButton` | FRIENDS |
| `Util_Help` | `UKodLabeledButton` | HELP |
| `Util_Menu` | `UKodLabeledButton` | MENU |
| `BriefingPanel` | `UWidget` | |
| `BriefingTitle` | `UCommonTextBlock` | BRIEFING |
| `Briefing_Patch` | `UCommonTextBlock` | Patch 0.9.2 notes |
| `Briefing_FeaturedMap` | `UCommonTextBlock` | Featured map: Desert Rail |
| `Briefing_Season` | `UCommonTextBlock` | Season 3 live |
| `Footer_Version` | `UCommonTextBlock` | v1 paint |
| `Footer_Online` | `UCommonTextBlock` | ONLINE |

`WBP_IdentityStrip` names: `Avatar`, `NameText`, `ClanTagText`, optional `LeagueLine` (leave it off the main menu). `FormatClanTag` wraps a bare tag as `[TAG]`. Placeholder identity is Commander Doe / `[WARD]`.

`WBP_LabeledButton` names: optional `Label`, optional `Icon`.

## Lobby Versus — `UKodLobbyVersusWidget` (cyan glass)

### Faction lock

`Column_Left` holds `FactionBoard` (`UKodFactionBoardWidget`). The board's only tiles are `FactionTile_USA` and `FactionTile_RSF`. `EKodVersusFaction` has those two enumerators. `GetVersusFactions` returns USA then RSF. `TrySetVersusFactionById` accepts the `FName`s `USA` and `RSF` and rejects everything else, including `RU`, `CN`, and `RANDOM`. Rejected ids are not hidden slots. Do not add a tile or accent material for `RU_RustIndustrial` or `CN_ImperialGreenGold`.

Each tile (`WBP_FactionTile`) names `IconImage`, `LabelText`, and `SelectionGlow`. `SetTileState` covers Idle, Hover, Selected, and Disabled. Hover does not override Selected or Disabled. A Disabled tile stays disabled when the board repaints, and it cannot become the selected faction. The board is cyan glass. USA starts Selected. RSF starts Idle.

### Chat on the right

`ChatRail` (`UKodLobbyChatRailWidget`) is a **direct** child of `LobbyColumns` and is forced to the last slot. `GetRequiredChatRail()` is `Right`. There is no left-rail value. Friends / Help / Menu stay icon-only on the lower left (`SetShowLabel(false)`), not inside the rail. The rail stays quiet cyan glass, secondary to orange PLAY RANKED.

Component sheet v1 names inside `WBP_LobbyChatRail`:

| Name | Type | Notes |
|------|------|-------|
| `ChatRoot` | `UBorder` | Quiet panel fill. Vertical stack lives inside it. |
| `ChannelTabs` | `UWidget` | Host for the two tabs. |
| `Tab_Lobby` | `UKodLabeledButton` | LOBBY. Default active. |
| `Tab_Party` | `UKodLabeledButton` | PARTY. |
| `Underline_Lobby` / `Underline_Party` | `UImage` | Optional cyan underline for the active tab. |
| `MessageList` | `UScrollBox` | History. |
| Message rows | `UKodChatMessageRowWidget` | Spawned from `MessageRowClass`. `SpeakerText` cyan, `BodyText` white. |
| `ChatInput` | `UWidget` | Input bar container. |
| `Text_Message` | `UEditableText` | Hint: Type message... |
| `Btn_Send` | `UKodLabeledButton` | Cyan send. Not orange. |

| Column (left → right) | Name | Contents |
|-----------------------|------|----------|
| Left | `Column_Left` | `Identity`, `FactionBoard`, utilities |
| Center | `Column_Center` | Season strip, PLAY RANKED, READY, UNRANKED, MAPS, map chip |
| Ladder | `Column_Ladder` | `LadderHeader`, `LadderList` |
| Chat | `ChatRail` | ChatRoot on the right |

### Other lobby slots

| Name | Type | Copy / behavior |
|------|------|-----------------|
| `HeroArt` | `UImage` | Backdrop |
| `GlassPlate` | `UImage` | Optional |
| `TitleText` | `UCommonTextBlock` | Warden Versus |
| `VersionBadge` | `UCommonTextBlock` | v2.6 |
| `Nav_Campaign` | `UKodLabeledButton` | CAMPAIGN (dim) |
| `Nav_Coop` | `UKodLabeledButton` | CO-OP (dim) |
| `Nav_Versus` | `UKodLabeledButton` | VERSUS (active) |
| `Nav_Custom` | `UKodLabeledButton` | CUSTOM (dim) |
| `Nav_Collection` | `UKodLabeledButton` | COLLECTION (dim) |
| `Nav_Replays` | `UKodLabeledButton` | REPLAYS — UNSHIPPED, disabled |
| `Mode_Training` | `UKodLabeledButton` | TRAINING |
| `Mode_OneVOne` | `UKodLabeledButton` | 1V1 (default) |
| `Mode_Teams` | `UKodLabeledButton` | TEAMS |
| `Mode_Tournaments` | `UKodLabeledButton` | TOURNAMENTS |
| `SeasonTitle` | `UCommonTextBlock` | SEASON 3 until `SetSeasonStrip` |
| `RankName` | `UCommonTextBlock` | DIAMOND RANK |
| `RecordLine` | `UCommonTextBlock` | Wins / losses |
| `Button_PlayRanked` | `UKodLabeledButton` | PLAY RANKED, orange |
| `ReadyStatus` | `UCommonTextBlock` | READY 1/1 |
| `Button_Unranked` | `UKodLabeledButton` | UNRANKED, cyan |
| `Button_Maps` | `UKodLabeledButton` | MAPS, cyan |
| `Button_MapChip` | `UKodLabeledButton` | DESERT RAIL |
| `LadderHeader` | `UCommonTextBlock` | RANK NAME MMR W L |
| `LadderList` | `UVerticalBox` | Rows from `LadderRowClass` |
| `LadderPreview` | `UCommonTextBlock` | Optional plain-text fallback |
| `Util_Friends` / `Util_Help` / `Util_Menu` | `UKodLabeledButton` | Icons only |
| `MatchmakingStatus` | `UCommonTextBlock` | Quiet MATCHMAKING ACTIVE |

`WBP_LadderRow` names: `RankText`, `NameText`, `MmrText`, `WinsText`, `LossesText`. Assign the row class on the lobby. The local row is the orange line. `GetStampPreviewRows` is fictional stamp copy for the designer, not a ranked service.

## HUD accents — USA HOLD, RSF v3 kept, CN/RU v2 kept

`W_HUDShell` is `UKodHudRootWidget` (`WBP_KodHUD`). Layout v4 weights and the 5×3 card are unchanged. Portrait geometry stays the v1.1 pin. No cyan glass on the HUD. HP stays green. Do not bake sheet labels such as "Portrait 15%" into widgets.

`ApplyFactionAccentTheme` loads `GetHudAccentColors`. Theme ids stay `USA_TanGreenGold` (alias `USA_Ironstock`) and `RSF_MetalStoneOrangeArch` (alias `RSF_RustOrange`).

**USA accent paint is HOLD** pending USA v4 QA PASS. `USA_TanGreenGold` is not authoritative. Tan/patriotic v3 is not the lock. Olive drab, gunmetal gray, and gold are the in-progress direction and are not implemented here. Soft Dev does not retint USA until that sheet passes.

**RSF v3 stays PASS.** `Frame` is sandstone, `Field` is warm adobe mortar, `Proud` is accent orange. Gothic black iron is gone. Cause-safe: no faith text, scripture, calligraphy, or mosque labels. No new arch widget.

**CN and RU stay stamped v2.** Their color tables are unchanged. They are not Versus tiles. `AccentForFaction` maps Versus USA and RSF only. `FactionBoard` still rejects CN, RU, and RANDOM. `IsRejectedHudAccentId` still rejects `RU_RustIndustrial` and `CN_ImperialGreenGold`.

| Theme id | Status | Language in this branch |
|----------|--------|-------------------------|
| `USA_TanGreenGold` | **HOLD** pending USA v4 QA. Versus USA | Not a lock. v3 tan/patriotic is not authoritative. v4 olive/gunmetal/gold is not landed |
| `RSF_MetalStoneOrangeArch` | **v3 PASS.** Versus RSF | Sandstone frame, adobe mortar, orange. Cause-safe. No arch widget |
| `CN_JadeStoneGoldRed` | Stamped v2 **PASS / KEEP**. Preview only | Jade, stone, gold, red |
| `RU_SovietColdBlueIce` | Stamped v2 **PASS / KEEP**. Preview only | Cold metal, ice blue |
| `RU_RustIndustrial` / `CN_ImperialGreenGold` | Rejected sheet names | None |

**Map-tool placement is HOLD** pending the updated template plate QA PASS. The next plate puts the three buttons on the right of the minimap and lets the map fill the bay flush with the box. That move is not in this branch. The header `ToolChipStrip` is not the locked placement. Column weight stays 18.

Chat stays the right rail. `FactionBoard` stays `FactionTile_USA` and `FactionTile_RSF`. Portrait geometry v1.1 is unchanged: `Img_Portrait` edge-to-edge, `Prog_Health` on the art-box bottom, `Slot_Energy` fixed under HP, one `Txt_Callsign` reading **Black Widow**.

Call `ApplyFactionAccentTheme` from match setup for USA or RSF, and from a preview hook for CN or RU. It does not add a Versus tile.

Handoff region names map onto the existing shell slots. Those slot names are not renamed:

| Handoff | Existing BindWidget |
|---------|---------------------|
| `W_HUDShell` | `UKodHudRootWidget` |
| `W_ResourceStrip` | `ResourceBar` |
| `W_MinimapCluster` | `Minimap` (column weight 18). Tool placement **HOLD** pending the updated template plate |
| `W_SelectionPanel` | `SelectionPanel` (42) |
| `W_PortraitPanel` | `Portrait` (15). v1.1 geometry: art fill, HP on the art bottom, ENERGY slot under HP |
| `W_CommandCard` | `CommandCard` (20, 5×3) |
| `W_HUDUtilityCluster` | `ChatMenuCluster` |

`Txt_Callsign` is the only Black Widow label, including when the RU preview theme is applied. `AccentForFaction` maps USA → `USA_TanGreenGold` and RSF → `RSF_MetalStoneOrangeArch`.

## Editor pass (still required)

1. Create the Widget Blueprints in the table above. Do not commit them.
2. Author Ironstock and cyan-glass materials. Hook them to Common Button styles. Drop the C++ tint once a style exists.
3. Paint icon brushes: credits, oil, supply, idle worker, army, ping, faction marks, send.
4. Assign hero textures.
5. Set `AKodHUD.HudWidgetClass` to `WBP_KodHUD`.
6. Push `WBP_MainMenu` and `WBP_LobbyVersus` on a CommonUI activatable stack (front-end only).
7. Wire `OnCommandRequested` to `UKodCommandSubsystem`. Wire resource setters to the wallet. Wire `SetMatchClock` / `SetVitals` from match state.
8. Call `CloseBuildSubmenu` from Esc while the build page is open.

## Checks before calling a Blueprint done

- HUD uses Ironstock materials only. No cyan panes on `WBP_KodHUD`.
- Main Menu and Lobby use cyan glass only. No riveted metal.
- Main Menu has one orange control: PLAY VERSUS.
- Lobby orange is PLAY RANKED plus the local ladder row.
- Lobby faction widgets are USA and RSF. No third tile.
- `ChatRail` is the rightmost column of `LobbyColumns`. Its material is cyan glass, not Ironstock.
- Command card has no title. Top row glyphs are Q W E R T. Selection has no title. Grid is 8×3.
- `Img_Portrait` fills the art box with no margin. `Prog_Health` is on that art bottom, not a separate bay. `Slot_Energy` keeps `PortraitEnergySlotHeightPx` when `Prog_Energy` is `Hidden`.
- `Txt_Callsign` is the only Black Widow text. No second copy of the label.
- Portrait geometry v1.1 is unchanged. Map-tool placement is **HOLD** pending the updated template plate. Do not treat the header strip as the lock, and do not move the tools to the right until that plate passes.
- USA accent paint is **HOLD** pending USA v4 QA. RSF stays v3 PASS. CN and RU stay stamped v2 previews, not Versus tiles. `RU_RustIndustrial` and `CN_ImperialGreenGold` stay rejected names.
- No binary UI assets in git.
