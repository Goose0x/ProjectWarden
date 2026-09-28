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
| Portrait panel pin | `W_PortraitPanel`: image fills the frame edge-to-edge. HP is flush to the bottom of that frame. ENERGY stays under HP. Hide with `Hidden`, never `Collapsed`. |
| HUD accents v1 | **HOLD.** `USA_Ironstock` / `RSF_RustOrange` are id stubs only. Re-skin paint is not final (portrait v1.1 in flight). **CONDITIONAL/FAIL:** do not wire `RU_RustIndustrial` or `CN_ImperialGreenGold`. |

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

Clock above/left, fixed-width digits. Tools are a vertical stack beside the square map: idle worker, army, ping. Optional size boxes enforce a 32px floor (24px minimum).

| Name | Type | Required |
|------|------|----------|
| `ClockText` | `UCommonTextBlock` | yes |
| `Size_Clock` | `USizeBox` | no |
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

Director pin. Parent the named widgets to a canvas (the Widget Blueprint root). `ApplyPortraitGeometry` runs in pre-construct and enforces:

1. `PortraitImage` fills the portrait frame edge-to-edge (anchors stretch, margin 0, brush margin 0).
2. `HPBar` is flush to the bottom of that frame (full width, bottom offset 0, height `PortraitHpBarHeightPx`). Green. `HPValue` sits on that bottom edge.
3. `EnergyCaption` then `EnergyBar` sit under the frame, directly beneath HP. The block height is reserved (`PortraitEnergyCaptionHeightPx` + `PortraitEnergyBarHeightPx`). Unused energy uses `Hidden`, not `Collapsed`, so the slot is not given away.

| Name | Type | Notes |
|------|------|-------|
| `PortraitImage` | `UImage` | Fills the frame. No inset. |
| `HPBar` | `UProgressBar` | Green. Bottom edge of the frame. |
| `HPValue` | `UCommonTextBlock` | `current/max`, on the HP bar. |
| `EnergyCaption` | `UCommonTextBlock` | Reads ENERGY. Under HP. |
| `EnergyBar` | `UProgressBar` | Under the caption. Reserved even when hidden. |

Paint on this panel stays Ironstock paint v3. Accent re-skin color is not applied here.

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

## HUD accents — HOLDs, shell unchanged

`W_HUDShell` is `UKodHudRootWidget` (`WBP_KodHUD`). Layout v4 weights and the 5×3 card are unchanged. No cyan glass on the HUD. HP stays green.

Two holds:

1. **RU_RustIndustrial and CN_ImperialGreenGold — CONDITIONAL/FAIL.** Do not add theme ids, enumerators, materials, or colors for those sheets. `IsRejectedHudAccentId` returns true for both names. The Versus roster stays USA · RSF only.
2. **USA / RSF re-skin paint — HOLD.** Portrait v1.1 revisions are in flight. `USA_Ironstock` and `RSF_RustOrange` stay as theme-id stubs already on the shell. `ApplyFactionAccentTheme` stores the id and does not retint. Do not treat the current accent stamp art as final paint. `GetHudAccentColor` and `GetIronstockRustOrange` return Ironstock paint v3 amber while this hold is in place.

| Theme id | What is hooked now | Paint |
|----------|--------------------|-------|
| `USA_Ironstock` | Id stored. `AccentForFaction(USA)` | Ironstock paint v3 amber on metal |
| `RSF_RustOrange` | Id stored. `AccentForFaction(RSF)` | Same paint v3. Re-skin not applied |
| `RU_RustIndustrial` | Not an id. Do not author a material. | None |
| `CN_ImperialGreenGold` | Not an id. Do not author a material. | None |

Chat stays the right rail. `FactionBoard` stays `FactionTile_USA` and `FactionTile_RSF`. The portrait pin stays: image edge-to-edge, HP flush to the bottom of the frame, ENERGY under HP with `Hidden` when unused.

Call `ApplyFactionAccentTheme` from match setup when the faction is known. It is a stub until the USA/RSF paint hold lifts.

Handoff region names map onto the existing shell slots. Those slot names are not renamed:

| Handoff | Existing BindWidget |
|---------|---------------------|
| `W_HUDShell` | `UKodHudRootWidget` |
| `W_ResourceStrip` | `ResourceBar` |
| `W_MinimapCluster` | `Minimap` (column weight 18) |
| `W_SelectionPanel` | `SelectionPanel` (42) |
| `W_PortraitPanel` | `Portrait` (15). Image edge-to-edge, HP flush to the frame bottom, ENERGY reserved under HP |
| `W_CommandCard` | `CommandCard` (20, 5×3) |
| `W_HUDUtilityCluster` | `ChatMenuCluster` |

If a portrait shows a callsign, the copy is **Black Widow** (`CallsignText`, optional). `AccentForFaction` maps USA → `USA_Ironstock` and RSF → `RSF_RustOrange`.

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
- `PortraitImage` has no margin. `HPBar` is flush to the bottom of the portrait frame. `EnergyBar` is under HP. Unused energy is `Hidden`, not `Collapsed`.
- Accent theme ids are `USA_Ironstock` or `RSF_RustOrange` only, and their re-skin paint is on HOLD (portrait v1.1 in flight). The HUD still shows Ironstock paint v3. RSF does not change column weights.
- `RU_RustIndustrial` and `CN_ImperialGreenGold` are CONDITIONAL/FAIL. No theme id, no material, no Versus tile.
- No binary UI assets in git.
