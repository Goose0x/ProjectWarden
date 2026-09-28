# UI / HUD — Ironstock

In-game only. Amber on metal. Do not use the cyan-glass materials from `/Game/UI/Styles/CyanGlass/`.

Create these Widget Blueprints in the Editor. **Do not commit `.uasset` files.**

| Asset | Parent | Path |
|-------|--------|------|
| `WBP_KodHUD` | `UKodHudRootWidget` | `/Game/UI/HUD/WBP_KodHUD` |
| `WBP_ResourceBar` | `UKodResourceBarWidget` | `/Game/UI/HUD/WBP_ResourceBar` |
| `WBP_Minimap` | `UKodMinimapWidget` | `/Game/UI/HUD/WBP_Minimap` |
| `WBP_SelectionPanel` | `UKodSelectionPanelWidget` | `/Game/UI/HUD/WBP_SelectionPanel` |
| `WBP_SelectionCell` | `UKodSelectionCellWidget` | `/Game/UI/HUD/WBP_SelectionCell` |
| `WBP_PortraitFrame` | `UKodPortraitFrameWidget` | `/Game/UI/HUD/WBP_PortraitFrame` |
| `WBP_CommandCard` | `UKodCommandCardWidget` | `/Game/UI/HUD/WBP_CommandCard` |
| `WBP_CommandSlot` | `UKodCommandSlotButton` | `/Game/UI/HUD/WBP_CommandSlot` |
| `WBP_ChatMenuCluster` | `UKodChatMenuClusterWidget` | `/Game/UI/HUD/WBP_ChatMenuCluster` |

Assign `WBP_KodHUD` (`W_HUDShell`) to `AKodHUD.HudWidgetClass`.

`ApplyFactionAccentTheme` still uses `USA_TanGreenGold` and `RSF_MetalStoneOrangeArch`, but that paint is an interim hold until USA/RSF v3 and the minimap template pass QA. Do not shrink map tools yet. `CN_JadeStoneGoldRed` and `RU_SovietColdBlueIce` stay stamped v2 previews, not Versus factions. Column weights stay 18 / 42 / 15 / 20 / 5. Portrait geometry stays v1.1.

`WBP_PortraitFrame` (`W_PortraitPanel`, geometry PASS v1.1): `Img_Portrait` fills the art box, `Prog_Health` sits on that art bottom, `Slot_Energy` keeps a fixed height under HP, and `Txt_Callsign` is the only Black Widow label.

BindWidget names, the 28% band, and the 18/42/15/20/5 columns are in [docs/ui/UE_CONSTRUCTION.md](../../../../../docs/ui/UE_CONSTRUCTION.md).

Resource brushes: credits mark, oil drop, people mark. Map tools: idle worker, army, ping. No command-card or control-group titles.
