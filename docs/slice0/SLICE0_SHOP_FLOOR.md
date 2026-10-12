# Slice 0 — Shop floor (Lead Software Dev + Unreal Engine SME)
**Clock:** weekend · **Gate:** six DataAssets + `L_Slice0` walk · **Sim truth:** ~16 Hz  
**Owners:** Lead + SME (Director PINNED)  
**Ping Ops (KoD Sync)** only on block or gate PASS.

## Law — exact asset ids (Primary Asset Name / FName = these strings)

| Id | Class | Path |
|----|-------|------|
| `RangerRifle` | `UKodWeaponDefinition` | `/Game/Warden/Data/` |
| `Ranger` | `UKodUnitDefinition` | `/Game/Warden/Data/` |
| `Dozer` | `UKodUnitDefinition` | `/Game/Warden/Data/` |
| `CommandCenter` | `UKodBuildingDefinition` | `/Game/Warden/Data/` |
| `Barracks` | `UKodBuildingDefinition` | `/Game/Warden/Data/` |
| `USA` | `UKodFactionDefinition` (thin PDA — add if missing) | `/Game/Warden/Data/` |

No `DA_` prefix. No `/Game/Units/Definitions/` for this gate. Filename = id (`Ranger.uasset`).

## Build time law
`BuildTicks = seconds × 16` (SimHz). Prefer `BuildTicks` on defs; if field is still `BuildTimeSeconds`, convert at enqueue: `Ticks = FMath::Max(1, FMath::RoundToInt(Seconds * 16))`.

## Authoring without a live editor (SME call)

**Do both:**

1. **AssetManager (content law)** — `DefaultGame.ini` Primary Asset Types for `KodWeaponDefinition`, `KodUnitDefinition`, `KodBuildingDefinition`, `KodFactionDefinition`, scan `/Game/Warden/Data`. When editor is up: right-click → Miscellaneous → Data Asset → class → **Save As exact id** under that folder.

2. **Runtime bootstrap (until .uassets exist)** — `UKodSlice0Bootstrap` (or Dev settings) `NewObject<U…>(Outer, TEXT("Ranger"))` etc. with RF_Public, fill fields from the sheet below, register with Asset Manager / a `UKodDataCatalog` soft map. PIE and automated tests use bootstrap when `SoftObjectPath(/Game/Warden/Data/Ranger)` fails to load. When real assets land, bootstrap yields to cooked assets (same FNames).

Do **not** rely on DefaultEngine alone without either bootstrap or real uassets — PrimaryAssetTypes only discover existing assets.

Editor recipe (when UE opens): create six assets → set fields → Save. One Blutility later can stamp the sheet.

## Movement without NavMesh-as-truth (SME call)

| Layer | Owns |
|-------|------|
| **Sim (16 Hz)** | Position, yaw, current order, arrival. Seek toward goal; stop inside acceptance radius (`Radius * 0.5`). Optional simple separation. |
| **Command** | Right-click ground or a non-sim prop → `Move` to the cursor **ImpactPoint** via `UKodCommandSubsystem` (not AI MoveTo, never the actor pivot). Right-click a sim entity that is not the current selection → `Attack`. |
| **Presentation** | Actor interpolates between last/current sim pose. No CharacterMovement driving match state. |
| **NavMesh** | Forbidden as truth this slice. Later: static obstacle query / flow field only — never Path Following Component as sim. |

**Hash-stable idle:** when order is None, do not integrate micro-velocity; snap/quantize pose (e.g. mm or fixed-point) so idle hash does not drift frame-to-frame.

**L_Slice0 walk:** SC2 camera (edge, arrows, middle-mouse, wheel; no WASD) · left select/box · right move · one Ranger walks · idle hash stable · HP from DataAsset only (no hardcoded HP in unit Tick).

**Box select:** LMB drag past 6px replaces the selection with every sim-registered actor whose viewport projection lies in the rect. Left Shift adds, same as click. A shorter drag stays click-select. `AKodHUD` draws the rect with the engine canvas (no Content asset).

## Field sheet (TEMP balance)

### RangerRifle
DisplayName Ranger Rifle · Damage 12 · Range 900 · CooldownSeconds 0.35

### Ranger
Tag `Kod.Unit.Ranger` · MaxHealth 120 · Armor 0 · MoveSpeed 450 · Sight 1600 · PrimaryWeapon → RangerRifle · Cost 75 Credits / 0 Luminene / supply 1 · BuildTicks 80 (5×16)

Presentation only (not a seventh Data id): after local import, set the Ranger DA **Static Mesh** to `/Game/Warden/Characters/USA/Ranger/SM_Ranger_Body.SM_Ranger_Body`. Mount folder is `/Game/Warden/Characters/USA/Ranger/`, not `/Game/Units/Meshes/`. Bootstrap leaves the soft ref null (Engine cube). Skeletal mesh stays empty until anims land. See `docs/04-slice0-warden.md`.

### Dozer
Tag `Kod.Unit.Dozer` · display AMP · MaxHealth 200 · Armor 1 · MoveSpeed 350 · Sight 1200 · no weapon · Cost 50 Credits / 0 Luminene / supply 1 · BuildTicks 192 (12×16). Shipped body is the tan cube. Skeletal mesh `SK_truck_a` is optional on the DataAsset and is not assigned by bootstrap.

### CommandCenter
Tag `Kod.Building.CommandCenter` · MaxHealth 2500 · Cost 400 Credits · BuildTime 71 s · SupplyProvided 15 · drop-off · PowerProvided 10 · Sight 2000 · Trainable: AMP (Dozer). Default mesh soft-ref `SM_general`. Barracks owns Ranger and is not trained in M1.

### Barracks
Tag `Kod.Building.Barracks` · MaxHealth 1500 · Cost 150 Credits · BuildTime 46 s · PowerConsumed 2 · Sight 1400 · Trainable: Ranger. Not placed or trained in M1.

### SupplyDepot
Tag `Kod.Building.SupplyDepot` · MaxHealth 400 · Cost 100 Credits · BuildTime 21 s · SupplyProvided 8 · mesh soft-ref `SM_storage`. Catalog only. Not spawned.

### USA
StartingBuilding CommandCenter · StartingUnits: 1 Dozer + 2 Ranger (PROXY)

## Playtest #1 economy (2026-09-29)

The match bank is the sim: Jadeite (HUD label **Credits**) and **Luminene** (gas from fissure vents, tag `Kod.Resource.Luminene`). `FKodTrainCost.Credits` spends Jadeite. `UKodResourceWallet` mirrors team 0 and is not a second ledger. This sheet's Luminene column is 0 for the units above.

Supply used/cap is on the sim. HQ provides 15, a depot provides 8, hard cap **200**. The HUD designer preview 48/60 is art and is not that cap.

**Dust gather collision (do not conflate):** spend fields that were `BuildCostDust` are Cash. World gather nodes are not renamed to Cash. This repo has no Dust-named gather node type; `UKodGatherComponent` stays a generic gather stub. Dust Storm (`Kod.Power.DustStorm`) and the Kingdom of Dust title stay. Purple-black / orange-red Dust node art is not in this lock. Escalate to PM if a design sheet still calls the spend crystal Dust.

## Ownership
- **Lead:** C++ sim fixed tick, bootstrap or uassets, select/move, L_Slice0 PIE, faction class
- **SME:** this law sheet, review, walk checklist
- **Ops:** ping only on block or PASS
