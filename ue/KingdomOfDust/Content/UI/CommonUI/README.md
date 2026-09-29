# UI / CommonUI

Shared front-end and HUD pieces. Screen roots are CommonUI activatable widgets (`UKodHudRootWidget`, `UKodMainMenuWidget`, `UKodLobbyVersusWidget`). Push the menu and lobby on an activatable stack in the Editor. The HUD is added by `AKodHUD`.

Create in the Editor. **Do not commit `.uasset` files.**

| Asset | Parent | Path |
|-------|--------|------|
| `WBP_LabeledButton` | `UKodLabeledButton` | `/Game/UI/CommonUI/WBP_LabeledButton` |
| `WBP_IdentityStrip` | `UKodIdentityStripWidget` | `/Game/UI/CommonUI/WBP_IdentityStrip` |

`WBP_LabeledButton` optional names: `Label`, `Icon`.
`WBP_IdentityStrip` names: `Avatar`, `NameText`, `ClanTagText`, optional `LeagueLine`.

See [docs/ui/UE_CONSTRUCTION.md](../../../../../docs/ui/UE_CONSTRUCTION.md).
