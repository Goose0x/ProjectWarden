# UI / Lobby Versus — cyan glass

Versus destination. Not Ironstock. Chat is the **right** rail. Factions are **USA** and **RSF** only.

Create in the Editor. **Do not commit `.uasset` files.**

| Asset | Parent | Path |
|-------|--------|------|
| `WBP_LobbyVersus` | `UKodLobbyVersusWidget` | `/Game/UI/Lobby/WBP_LobbyVersus` |
| `WBP_LobbyChatRail` | `UKodLobbyChatRailWidget` | `/Game/UI/Lobby/WBP_LobbyChatRail` |
| `WBP_LadderRow` | `UKodLadderRowWidget` | `/Game/UI/Lobby/WBP_LadderRow` |

Do not add RU, CN, or RANDOM tiles. Do not parent `ChatRail` anywhere but as the last direct child of `LobbyColumns`.

Orange: **PLAY RANKED** and the local ladder row. Badge copy is **v2.6**.

Slot names: [docs/ui/UE_CONSTRUCTION.md](../../../../../docs/ui/UE_CONSTRUCTION.md).
