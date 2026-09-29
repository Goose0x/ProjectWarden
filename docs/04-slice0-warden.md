# Slice 0 — Warden Data + walk gate

**Status:** SME **CONDITIONAL PASS** on C++/catalog (2026-09-26) · gate OPEN until PIE walk  
**Engine:** UE **5.8** (`KingdomOfDust.uproject` EngineAssociation)  
**Law sheet:** `docs/slice0/SLICE0_SHOP_FLOOR.md`  
**Clock:** 2026-09-26 PT

## Exact id law (bare FNames — no `DA_` prefix)

| Id | Class | Soft path |
|----|-------|-----------|
| `RangerRifle` | `UKodWeaponDefinition` | `/Game/Warden/Data/RangerRifle.RangerRifle` |
| `Ranger` | `UKodUnitDefinition` | `/Game/Warden/Data/Ranger.Ranger` |
| `Dozer` | `UKodUnitDefinition` | `/Game/Warden/Data/Dozer.Dozer` |
| `CommandCenter` | `UKodBuildingDefinition` | `/Game/Warden/Data/CommandCenter.CommandCenter` |
| `Barracks` | `UKodBuildingDefinition` | `/Game/Warden/Data/Barracks.Barracks` |
| `USA` | `UKodFactionDefinition` | `/Game/Warden/Data/USA.USA` |

Constants: `KodWardenPaths.h`. Catalog JSON: `Content/Warden/Data/WardenSlice0Catalog.json`.

## BuildTicks formula

```
BuildTicks = FMath::RoundToInt(Seconds * 16.f)   // SimHz = 16
```

Helper: `KodBuildTicks::SecondsToBuildTicks` / `BuildTicksToSeconds` / `ResolveBuildTicks`.

| Asset | Seconds | BuildTicks |
|-------|---------|------------|
| Ranger | 5 | **80** |
| Dozer | 8 | **128** |
| Barracks | 20 | **320** |
| CommandCenter | 0 | **0** |

## Dual authoring (must do both)

1. **AssetManager** — `Config/DefaultGame.ini` scans `/Game/Warden/Data` for `KodWeaponDefinition`, `KodUnitDefinition`, `KodBuildingDefinition`, `KodFactionDefinition`.
2. **`UKodSlice0Bootstrap`** — `NewObject` with exact FNames above, shop-sheet fields, register in `UKodDataCatalog`. PIE: SoftObjectPath first; on miss → catalog.

## How to create the six DAs in editor

1. Open project in UE **5.8**.
2. Content Browser → `/Game/Warden/Data`.
3. Right-click → **Miscellaneous → Data Asset** → class from table → **Save As exact id** (`RangerRifle`, not `DA_Weapon_RangerRifle`).
4. Fill from matching `Content/Warden/Data/<Id>.md` or catalog JSON.
5. Wire soft refs (Ranger → RangerRifle, Barracks Trainable → Ranger, USA start refs, etc.).

**Do not commit fake binary `.uasset` / `.umap` files.**

## Sim + movement law

- `UKodSimSubsystem`: fixed step **SimHz=16**, catch-up capped (`MaxCatchUpSteps=4`).
- Sim owns pose + orders; actors interpolate.
- Seek toward goal; stop inside `AcceptanceRadius * 0.5`.
- **NavMesh forbidden** this slice. No AI MoveTo / PathFollowing / CharacterMovement as match truth.
- Idle: no micro-integrate; quantize pose; `ComputeIdleHash()` stable when `IsWorldIdle()`.
- Hitscan rifle (no actor bullets); HP from DataAsset via `ConfigureEntity` / `ApplyDefinition`.

## Walk-gate classes

| Role | Class |
|------|-------|
| Camera | `AKodRTSCameraPawn` |
| PC (base) | `AKodPlayerController` (select / marquee / mouse fallback) |
| PC (command path) | `AKodSlice0PlayerController` → `UKodCommandSubsystem` |
| GameMode | `AKodSlice0GameMode` (bootstrap + optional smoke Ranger) |
| Move bridge | `UKodMoveComponent` → `Sim->IssueMove` |
| Attack bridge | `UKodAttackComponent` → `Sim->IssueAttack` (hitscan) |
| Spawn | `UKodSlice0Bootstrap::SpawnRanger` |
| Faction PDA | `UKodFactionDefinition` (not TechTree) |

## L_Slice0 acceptance checklist

- [ ] Map `Content/Maps/Sandbox/L_Slice0` (editor) with `AKodSlice0GameMode`
- [ ] Iso camera pans/zooms
- [ ] Left-click select + drag box select
- [ ] Right-click ground → Move through command subsystem / sim seek
- [ ] Right-click enemy → Attack hitscan (RangerRifle damage 12 / range 900)
- [ ] One Ranger walks; arrival stops in acceptance radius
- [ ] Idle hash stable across frames when no orders
- [ ] HP from DA (Ranger 120) — no hardcoded HP in unit Tick
- [ ] Soft path or bootstrap resolves all six ids

## Blockers

- Binary `.uasset` creation requires Unreal Editor (not on scaffold host).
- `L_Slice0.umap` must be created in editor (greybox + PROXY meshes).
- Enhanced Input IMC/IA assets still Content-only; mouse fallback works for greybox PIE.
- TEMP rifle audio cue path deferred (SFX wallet paused).


## SME review nits (non-blocking)

1. **Engine** — docs/project aligned to **UE 5.8** (was 5.4 wording).
2. **AcceptanceRadius** — Slice 0 still uses `UKodMoveComponent` / sim default `50`. `UKodUnitDefinition::CollisionRadius` added for a later data-driven stop distance.
3. **Hitscan vs Armor** — Slice 0 hitscan applies raw weapon Damage; Armor is stored on the DA but **not** applied in sim yet. Track for combat pass.
4. **Bootstrap soft weapon refs** — PIE resolves via catalog/`ResolveWeapon`; after editor stamps real `/Game/Warden/Data/*.uasset`, re-wire soft refs in Content Browser.

**Ops:** do **not** ping until walk checklist PASS (or a hard block).
