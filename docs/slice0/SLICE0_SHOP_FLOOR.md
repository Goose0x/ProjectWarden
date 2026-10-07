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

**L_Slice0 walk:** iso cam · left select/box · right move · one Ranger walks · idle hash stable · HP from DataAsset only (no hardcoded HP in unit Tick).

**Box select:** LMB drag past 6px replaces the selection with every sim-registered actor whose viewport projection lies in the rect. Left Shift adds, same as click. A shorter drag stays click-select. `AKodHUD` draws the rect with the engine canvas (no Content asset).

## Field sheet (TEMP balance)

### RangerRifle
DisplayName Ranger Rifle · Damage 12 · Range 900 · CooldownSeconds 0.35

### Ranger
Tag `Kod.Unit.Ranger` · MaxHealth 120 · Armor 0 · MoveSpeed 450 · Sight 1600 · PrimaryWeapon → RangerRifle · BuildCostCash 75 · BuildTicks 80 (5×16)

### Dozer
Tag `Kod.Unit.Dozer` · MaxHealth 200 · Armor 1 · MoveSpeed 350 · Sight 1200 · no weapon · BuildCostCash 100 · BuildTicks 128 (8×16)

### CommandCenter
Tag `Kod.Building.CommandCenter` · MaxHealth 2500 · BuildCostCash 0 preplaced · PowerProvided 10 · Sight 2000 · Trainable: Dozer (Barracks owns Ranger)

### Barracks
Tag `Kod.Building.Barracks` · MaxHealth 1500 · BuildCostCash 250 · BuildTicks 320 (20×16) · PowerConsumed 2 · Sight 1400 · Trainable: Ranger

### USA
StartingBuilding CommandCenter · StartingUnits: 1 Dozer + 2 Ranger (PROXY)

## Playtest #1 economy (2026-09-29)

Primary spend wallet is **Cash** (`BuildCostCash`, tag `Kod.Resource.Cash`). **Oil** (`Kod.Resource.Oil`) is the second store. This sheet has no Oil cost column. Do not use Credits.

Supply hard cap is **200** (`KodSupplyHardCap` on `UKodResourceWallet`). The HUD designer preview 48/60 is art and is not that cap.

**Dust gather collision (do not conflate):** spend fields that were `BuildCostDust` are Cash. World gather nodes are not renamed to Cash. This repo has no Dust-named gather node type; `UKodGatherComponent` stays a generic gather stub. Dust Storm (`Kod.Power.DustStorm`) and the Kingdom of Dust title stay. Purple-black / orange-red Dust node art is not in this lock. Escalate to PM if a design sheet still calls the spend crystal Dust.

## Ownership
- **Lead:** C++ sim fixed tick, bootstrap or uassets, select/move, L_Slice0 PIE, faction class
- **SME:** this law sheet, review, walk checklist
- **Ops:** ping only on block or PASS
