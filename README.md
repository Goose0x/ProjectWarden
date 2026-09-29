# ProjectWarden

Original Unreal Engine RTS. The game project in this repo is **Kingdom of Dust** (`Kod*`): a StarCraft 2–like chassis with a clean-room Generals-flavor command layer.

This repository does **not** include Command & Conquer, Generals, or Zero Hour game code, and it does not vendor any GPL reference tree. Study notes in `docs/` describe behaviors to reimplement in Unreal. Product names in code and UI are Kingdom of Dust / `Kod*` only.

## Layout

| Path | What it is |
|------|------------|
| [`ue/KingdomOfDust/`](ue/KingdomOfDust/) | Unreal **5.8** C++ project (`KingdomOfDust.uproject`) |
| [`docs/`](docs/) | Architecture, project skeleton, scaffold status, Slice 0 shop floor |

## Open in UE 5.8

1. Install **Unreal Engine 5.8**. That matches `EngineAssociation` in [`ue/KingdomOfDust/KingdomOfDust.uproject`](ue/KingdomOfDust/KingdomOfDust.uproject).
2. Open `ue/KingdomOfDust/KingdomOfDust.uproject` in the Unreal Editor. On Windows you can generate Visual Studio project files from that `.uproject` first; on Mac or Linux, use the Epic project browser.
3. The first open compiles the C++ modules: `KingdomOfDust`, `KodCore`, `KodUnits`, `KodEconomy`, `KodGenerals`, `KodAI`, `KodNet`, `KodUI`, and editor-only `KodEditor`.

Module map, default GameMode classes, and enabled plugins are in [`ue/KingdomOfDust/README.md`](ue/KingdomOfDust/README.md).

## Slice 0

C++ and the Warden catalog are a **conditional pass**. The playable gate is still open:

- Create the map **`L_Slice0`** in the editor (intended path `Content/Maps/Sandbox/L_Slice0`, game mode `AKodSlice0GameMode`).
- Run the PIE walk checklist in [`docs/04-slice0-warden.md`](docs/04-slice0-warden.md).

Law sheet for the six bare asset ids (`RangerRifle`, `Ranger`, `Dozer`, `CommandCenter`, `Barracks`, `USA`): [`docs/slice0/SLICE0_SHOP_FLOOR.md`](docs/slice0/SLICE0_SHOP_FLOOR.md).

## Content assets

`.uasset` and `.umap` files are **created in the editor**. Git tracks markdown placeholders under `Content/` plus the JSON catalog at `ue/KingdomOfDust/Content/Warden/Data/WardenSlice0Catalog.json`. Do not commit invented binary assets. `Binaries/`, `Intermediate/`, `DerivedDataCache/`, and `Saved/` stay untracked.

## Docs

- [Architecture (clean-room)](docs/01-zh-to-unreal-architecture.md)
- [UE project skeleton](docs/02-ue-project-skeleton.md)
- [Scaffold status](docs/03-scaffold-status.md)
- [Slice 0 — Warden](docs/04-slice0-warden.md)
- [BWAPI observe/command notes](docs/05-bwapi-to-kod.md)
- [Slice 0 shop floor](docs/slice0/SLICE0_SHOP_FLOOR.md)
