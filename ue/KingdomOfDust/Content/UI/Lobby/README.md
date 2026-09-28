# UI / Lobby Versus — cyan glass

Versus destination. Not Ironstock. Chat is the **right** rail. Factions are **USA** and **RSF** only.

Create in the Editor. **Do not commit `.uasset` files.**

| Asset | Parent | Path |
|-------|--------|------|
| `WBP_LobbyVersus` | `UKodLobbyVersusWidget` | `/Game/UI/Lobby/WBP_LobbyVersus` |
| `WBP_LobbyChatRail` | `UKodLobbyChatRailWidget` | `/Game/UI/Lobby/WBP_LobbyChatRail` |
| `WBP_ChatMessageRow` | `UKodChatMessageRowWidget` | `/Game/UI/Lobby/WBP_ChatMessageRow` |
| `WBP_FactionBoard` | `UKodFactionBoardWidget` | `/Game/UI/Lobby/WBP_FactionBoard` |
| `WBP_FactionTile` | `UKodFactionTileWidget` | `/Game/UI/Lobby/WBP_FactionTile` |
| `WBP_LadderRow` | `UKodLadderRowWidget` | `/Game/UI/Lobby/WBP_LadderRow` |

Chat rail inner names: `ChatRoot`, `ChannelTabs`, `Tab_Lobby`, `Tab_Party`, `MessageList`, `ChatInput`, `Text_Message`, `Btn_Send`. Message rows name `SpeakerText` and `BodyText`.

Faction board names: `FactionBoardRoot`, `FactionTile_USA`, `FactionTile_RSF`. Tile names: `IconImage`, `LabelText`, `SelectionGlow`.

Do not add RU, CN, or RANDOM tiles. Do not parent `ChatRail` anywhere but as the last direct child of `LobbyColumns`. Do not put Ironstock on the chat rail or the faction tiles.

Orange: **PLAY RANKED** and the local ladder row. Badge copy is **v2.6**.

Slot names: [docs/ui/UE_CONSTRUCTION.md](../../../../../docs/ui/UE_CONSTRUCTION.md).
