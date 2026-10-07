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

## Ranger body mesh (presentation)

The Ranger DataAsset stays `/Game/Warden/Data/Ranger.Ranger`. The mesh is not a new Primary Asset id. `AKodUnit` reads it off `UKodUnitDefinition` and draws it on the existing static mesh child `UnitMesh`.

Constants: `KodWardenPaths::RangerCharacterRoot`, `KodWardenPaths::RangerBodyStaticMesh`.

| Role | Soft path |
|------|-----------|
| Mount folder | `/Game/Warden/Characters/USA/Ranger/` |
| Slice 0 static mesh | `/Game/Warden/Characters/USA/Ranger/SM_Ranger_Body.SM_Ranger_Body` |
| Skeletal mesh | Leave empty until anims land |
| Rejected folder | `/Game/Units/Meshes/` |

`UKodSlice0Bootstrap::MakeRanger` leaves `SkeletalMesh` and `StaticMesh` null. PIE with only the catalog still shows `/Engine/BasicShapes/Cube` at scale `0.8×0.8×1.7`. A set soft path that fails to load does the same. PIE log: `KodUnitBody <name> Source=Cube` or `Source=StaticMesh`.

### Editor import (local only — no binaries in git)

1. Open `ue/KingdomOfDust/KingdomOfDust.uproject` in UE **5.8**.
2. Content Browser → add `/Game/Warden/Characters/USA/Ranger/` (this mount, not `Units/Meshes`).
3. Import the baked USA Ranger LOW FBX as a **static mesh**. Skeletal mesh and anims wait for a later pass.
4. If Import keeps the source asset name, rename the static mesh to **`SM_Ranger_Body`**. The soft path above has to match the package name.
5. Open `/Game/Warden/Data/Ranger` (create it with the six-DA recipe above if it is not there yet). Set **Static Mesh** to `SM_Ranger_Body`. Leave **Skeletal Mesh** empty.
6. Save. Do not commit the `.uasset`, the FBX, or the map.

On the next PIE, `ApplyDefinition` puts that mesh on `UnitMesh` at relative location `0` (still attached to the capsule) and logs `KodUnitBody … Source=StaticMesh`. `MountResolvedStaticMesh` then sets a uniform `RelativeScale3D` of `170 / (BoxExtent.Z * 2)` so the body is about 170 cm tall. That is a Slice 0 presentation band-aid: `SM_Ranger_Body` was imported in meters (bounds ~1.7 units, ~2 cm in Unreal) and disappears next to the cube at scale 1. Invalid bounds stay at scale 1. Remove the band-aid once Art reimports `SM_Ranger_Body` at centimeter scale, or once a skeletal mesh lands. The cube placeholder stays scale `0.8×0.8×1.7` at the same attachment.

Select collision is unchanged: after the mesh swap, `UnitMesh` is set again to the **Pawn** profile and **QueryOnly** (Visibility stays ignored by that profile). The capsule is not modified. Click-select and drag-select still use the same traces. The orange tint and custom-depth stencil `1` still walk visible mesh components, so the Ranger body highlights the same way the cube did. A skeletal mesh, when one is assigned later, shows on the Character mesh; `UnitMesh` is hidden but keeps this QueryOnly Pawn collision.

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
| PC (command path) | `AKodSlice0PlayerController` → `UKodCommandSubsystem` — `/Script/KingdomOfDust.KodSlice0PlayerController` |
| GameMode | `AKodSlice0GameMode` (bootstrap + optional smoke Ranger + PROXY collision mute) — `/Script/KingdomOfDust.KodSlice0GameMode` |
| Move bridge | `UKodMoveComponent` → `Sim->IssueMove` |
| Attack bridge | `UKodAttackComponent` → `Sim->IssueAttack` (hitscan) |
| Spawn | `UKodSlice0Bootstrap::SpawnRanger` |
| Faction PDA | `UKodFactionDefinition` (not TechTree) |

## Script path vs `KoD_alpha.uproject`

Open `ue/KingdomOfDust/KingdomOfDust.uproject` (UE 5.8). The `.uproject` file name is not a C++ module.

World Settings → GameMode Override: `/Script/KingdomOfDust.KodSlice0GameMode`.

A flat local `KoD_alpha.uproject` must list the **same Modules** as `KingdomOfDust.uproject` (`KingdomOfDust`, `KodCore`, `KodUnits`, `KodEconomy`, `KodGenerals`, `KodAI`, `KodNet`, `KodUI`, `KodEditor`) — not a single `KoD_alpha` module. Never `/Script/KoD_alpha.…` unless that module exists. A missing class falls the map back to `GameModeBase`. After the class path is fixed, delete and re-place PROXY actors (`KodProxyUnit_*`).

## L_Slice0 acceptance checklist

- [ ] Map `Content/Maps/Sandbox/L_Slice0` (editor). World Settings GameMode Override: `/Script/KingdomOfDust.KodSlice0GameMode`
- [ ] Iso camera pans/zooms
- [ ] Left-click select. `AKodSlice0GameMode::StartPlay` sets `NoCollision` on primitive components of actors whose name or label contains `PROXY_` (greybox stays placed and visible). PIE log: `Slice0 PROXY collision muted Count=N`. Those actors must not appear in the click HitLog. The ray still walks ECC_Pawn past any other unregistered blocker (ground) and selects the first sim-registered actor. A ray that only hits those remaining props clears selection. Drags shorter than `BoxSelectDragThresholdPx` (6) stay on this path. PIE log: `ClickSelectAtCursor LocalSelection=… Picked=… Hits=Actor(Pawn,sim=0|1,id=…,loc=…)`
- [ ] Drag-select. LMB drag at or past 6px shows an orange screen rect on `AKodHUD` (`DrawRect` / `DrawLine`, engine white texture, no Content asset). On release, every sim-registered actor whose viewport projection lies inside that rect is selected. A fresh drag replaces the selection; Left Shift adds (same as click). An empty box clears unless Shift is held. The orange tint updates for the whole set. PIE: `DragSelect LocalSelection=… Additive=0|1 Box=x0,y0-x1,y1 Hits=Name(id=…,screen=…,…)|none` then `SelectionHighlight LocalSelection=… Meshes=… Tint=1`
- [ ] Right-click empty ground or a non-sim prop (`StaticMeshActor_*` floor) → Move to the **first** cursor ImpactPoint (the destination is that ImpactPoint, not the actor pivot). `PROXY_*` meshes are collision-muted at Slice 0 start, so they are not that first Pawn hit. The ray walks past remaining unregistered Pawn blockers the same way click-select does: a sim-registered actor on the ray that is not the current selection → Attack. Self / selection is not an attack target (Move at the first ImpactPoint instead). PIE: `RMB Move LocalSelection=… Dest=x,y,z Hit=… sim=0|1` or `RMB Attack LocalSelection=… Target=… Id=…`, then `Move Issued Sources=… Dest=…` / `Attack Issued Sources=… Target=… Id=…`. `Attack Reject NonSim` means the pivot fallback did not run.
- [ ] Right-click enemy → Attack hitscan (RangerRifle damage 12 / range 900)
- [ ] Selected actors show an orange tint (engine `BasicShapeMaterial` `Color`, no Content asset) and custom-depth stencil `1` on their meshes. Both clear when the selection is cleared or replaced. `DefaultEngine.ini` sets `r.CustomDepth=3` so the stencil is written. PIE: `SelectionHighlight LocalSelection=… Meshes=… Tint=1`
- [ ] One Ranger walks; arrival stops in acceptance radius
- [ ] Idle hash stable across frames when no orders
- [ ] HP from DA (Ranger 120) — no hardcoded HP in unit Tick
- [ ] Soft path or bootstrap resolves all six ids
- [ ] Ranger body uses `/Game/Warden/Characters/USA/Ranger/SM_Ranger_Body` when that static mesh is imported and set on `/Game/Warden/Data/Ranger`. The static body is auto-scaled to about 170 cm from mesh bounds (meter-import band-aid). Until the mesh is set, the unit stays the Engine cube (`KodUnitBody … Source=Cube`). Select / move / drag-select stay on the existing QueryOnly Pawn body. PIE: `KodUnitBody … Source=StaticMesh Mesh=SM_Ranger_Body` and a visible human-sized Ranger.

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
