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

Select collision is unchanged: after the mesh swap, `UnitMesh` is set again to the **Pawn** profile and **QueryOnly** (Visibility stays ignored by that profile). The capsule is not modified. Click-select and drag-select still use the same traces. The rim overlay and custom-depth stencil `1` still walk visible mesh components, so the Ranger body highlights the same way the cube did. The body's own materials stay visible. A skeletal mesh, when one is assigned later, shows on the Character mesh; `UnitMesh` is hidden but keeps this QueryOnly Pawn collision.

## Sim + movement law

- `UKodSimSubsystem`: fixed step **SimHz=16**, catch-up capped (`MaxCatchUpSteps=4`).
- Sim owns pose + orders; actors interpolate.
- Seek toward goal; stop inside `AcceptanceRadius * 0.5`.
- **NavMesh forbidden** this slice. No AI MoveTo / PathFollowing / CharacterMovement as match truth.
- Idle: no micro-integrate; quantize pose; `ComputeIdleHash()` stable when `IsWorldIdle()`.
- Hitscan rifle (no actor bullets). HP comes from the DataAsset via `ConfigureEntity` / `ApplyDefinition`. Damage is raw weapon Damage (RangerRifle 12). Armor is stored and not applied. No projectile actor.
- **RangerRifle resolve.** `AKodUnit::ApplyDefinition` calls `UKodSlice0Bootstrap::ResolveRangerRifleCombatStats` for a Ranger (definition id or asset name `Ranger`) or any primary-weapon soft path whose asset name or subobject is `RangerRifle`. That loads `/Game/Warden/Data/RangerRifle`. On a miss the sim used to keep `WeaponDamage` and `WeaponRange` at 0: `StepEntityAttack` only applies a hit when `WeaponDamage > 0`, so the order was issued and then silently did nothing (a 0 range also makes the unit seek onto the target and never fire). The miss now fills a code default and logs **one warning per world**: `KodWeapon fallback RangerRifle Damage=12 Range=900 Cooldown=0.8125`. `0.8125` is 13 sim ticks (`RoundToInt(0.8 * 16)` at SimHz 16, times `SimDt`). The in-memory catalog stand-in uses the same numbers. Creating the Data Asset below replaces them.
- **Attack step.** While 2D distance is greater than `WeaponRange`, seek toward the target (same seek as Move, not AI MoveTo) and face it. Inside range, stop, set `YawDegrees` toward the target, and fire when `CooldownRemaining` hits 0. Cooldown then resets from the weapon and counts down by `SimDt` each step. Each shot logs `KodSim Fire Attacker=<name> Target=<name> Tick=<sim tick index>` then `KodSim Hit Target=<name> Id=<id> HP=<remaining>`. `Tick` is `SimTickIndex` at the shot (steps already completed; it increments after `StepSim`).
- **Kill.** The shot that reaches HP 0 logs `KodSim Kill Target=<name> Id=<id>` in that same step, clears the attacker's order, and `UnregisterEntity` on the target. The target is no longer selectable or attackable. It is not left registered at 0 HP. `UKodSimSubsystem` broadcasts `OnUnitFired`, `OnUnitHit`, and `OnUnitKilled` for presentation. Those listeners must not write sim pose, orders, or the idle hash. Removing the entity changes the hash; with no orders left it is stable again.
- `FKodSimEntityState::TeamId` is set from the actor in `ConfigureEntity` (local smoke Ranger and `AKodPlayerState` default **0**, hostile test Ranger **1**). It is not part of `ComputeIdleHash`.
- Presentation life (vertical bob, visual yaw) lives on `AKodUnit` and does not write sim pose, orders, or the idle hash.

## Placeholder life (presentation only)

`AKodUnit` ticks in `TG_PostUpdateWork`. The sim subsystem does not expose a tick-group override in UE 5.8, so this may run before or after pose sync. Bob and facing are `UnitMesh` relative offsets and still show either way (one frame late if the life tick runs first). While the actor is actually moving (horizontal speed at or above 200 cm/s, so a 1 uu idle quantize snap does not count), `UnitMesh` bobs on **relative location** — an offset on top of the existing relative-location-0 attach. It does not change `RelativeScale3D`.

| Tunable | Default | Role |
|---------|---------|------|
| `BobAmplitudeCm` | 3.5 | Centimetres, before the height scale |
| `BobFrequencyHz` | 2.5 | Walking cadence |
| `BobEaseSpeed` | 8 | Eases the bob in while moving and back to the rest attach when stopped |
| `TurnRateDegreesPerSecond` | 540 | `RInterpConstantTo` on body yaw (`UnitMesh` relative) |

The amplitude is multiplied by `(mesh local height × RelativeScale3D.Z) / 170`. The Engine cube (100 cm at scale 1.7) and `MountResolvedStaticMesh`'s ~170 cm Ranger both stay near `BobAmplitudeCm`. Sine weight eases to 0 at rest and the mesh relative location is put back on the captured attach point.

Visual yaw chases sim `YawDegrees` (movement facing, and the attack target while seeking or holding range) and is applied as `UnitMesh` relative yaw, not `SetActorRotation`. The sim value is not written. The actor yaw stays the sim facing. The attack step writes that sim yaw toward the target before the shot, so the body turns to face while it fires.

Once per unit at BeginPlay: `KodUnitLife <Name> Bob=3.5 Turn=540` (the tunables, not the height-scaled amplitude).

## Hostile test target

After PROXY mute and the smoke Ranger, `AKodSlice0GameMode::SpawnHostileTestTarget` spawns one more Ranger from the same definition on **team 1**. It issues no attack and has no AI.

Placement: actor tag `KodHostileAnchor` first, else an actor whose name or label is `PROXY_HOSTILE` (PIE object names ending in `_PROXY_HOSTILE` count). XY comes from that actor; Z is the smoke Ranger Z (`SmokeRangerOffset.Z`, default 100) so the label's text height is not used. If neither tag nor label exists, spawn at `(1200, 600, 100)`.

Visual: the hostile is **forced onto the Engine cube** and slot 0 is a dynamic instance of `/Engine/BasicShapes/BasicShapeMaterial` with `Color` and `BaseColor` set to red. `BasicShapeMaterial` honors `Color`. This does not use `SetOverlayMaterial` or custom-depth stencil, so the local selection rim (`M_SelectionRim`, stencil `1`) is untouched. The Ranger static mesh is skipped on this unit even when `/Game/Warden/Data/Ranger` has one, which keeps the tell working with no Content. `KodUnitBody … Source=Cube` on that spawn is expected.

Click-select and drag-select only take sim actors whose `TeamId` matches `AKodPlayerState::TeamId` (default 0). The hostile is walked past and gets no rim. RMB is unchanged: a sim actor that is not the current selection, including team 1, logs `RMB Attack` and then `Attack Issued`.

PIE: `Slice0 Hostile spawned Name=<name> Team=1 Loc=<x,y,z> Anchor=PROXY_HOSTILE` or `Anchor=fallback`.

Ranger HP is 120 and RangerRifle damage is 12, so the hostile reaches HP 0 after **10** hits. The first `KodSim Hit` is remaining HP **108**, then 96, 84, 72, 60, 48, 36, 24, 12, and **0** on the tenth. That tenth shot also logs `KodSim Kill` and `KodUnit Death <Name>`. The attacker clears the order in that same sim step. The hostile is unregistered, then its presentation sinks into the floor over about 1.5 s and the actor is destroyed. It does not stay in the world at 0 HP.

Smoke spawn `(400, 0, 100)` to the default hostile anchor `(700, 200, 100)` is about 360 cm, inside range 900, so the Ranger turns to face and fires in place. A target farther than 900 is seeked until it is in range, then the attacker stops and fires. The hostile still does not shoot back.

## Attack presentation (no Content)

Driven by `OnUnitFired` / `OnUnitHit` / `OnUnitKilled` on `UKodSimSubsystem`. `AKodUnit` binds in BeginPlay and unbinds in EndPlay. Nothing in this section writes sim state or the idle hash.

| Event | What you see |
|-------|----------------|
| Fire | The body is already turning onto sim yaw. A warm-orange `UPointLightComponent` (~0.06 s, no shadows) plus a small engine sphere and cone (`/Engine/BasicShapes/Sphere`, `Cone`, `BasicShapeMaterial` tinted orange) at the front of the body. A thin `/Engine/BasicShapes/Cylinder` tracer from that muzzle to the target's chest, `BasicShapeMaterial` tinted yellow, alive ~0.08 s, then hidden. Tagged `KodFx` so the selection rim skips them. |
| Hit | The target's `UnitMesh` slot 0 flashes (~0.1 s): white when the rest tint is red, red when the rest tint is light. A short knockback jiggle is added on `UnitMesh` relative location, on top of the bob offset. |
| HP bars | `AKodHUD` draws a ~60 px bar above each still-registered sim unit (capsule top, engine canvas, no Content texture). Team 0 is green, any other team is red, background is near-black. Full HP is hidden unless that unit is in the local selection. |
| Kill | Log `KodUnit Death <Name>`. Collision turns off (no longer a click or attack target; already unregistered). The actor sinks about 220 cm into the floor over ~1.5 s, then `Destroy()`. |

## Replace the RangerRifle fallback (editor)

When `/Game/Warden/Data/RangerRifle` loads, `ResolveRangerRifleCombatStats` uses that asset and does **not** log `KodWeapon fallback`. Create it with the six-DA recipe above.

| Field | Value |
|-------|--------|
| Class | `UKodWeaponDefinition` |
| Script class | `/Script/KodUnits.KodWeaponDefinition` |
| Asset name | `RangerRifle` (bare id, no `DA_` prefix) |
| Soft path | `/Game/Warden/Data/RangerRifle.RangerRifle` |

Properties to set (these are the `UPROPERTY` names):

| Property | Type | Set to |
|----------|------|--------|
| `DefinitionId` | FName | `RangerRifle` |
| `DisplayName` | FText | `Ranger Rifle` |
| `Damage` | float | `12` |
| `Range` | float | `900` |
| `CooldownSeconds` | float | `0.35` |

`CooldownSeconds` `0.35` is the shop-sheet value. It replaces the code fallback of 13 sim ticks (0.8125 s). The sim subtracts `SimDt` (1/16 s) from `CooldownRemaining` each step. Also set the Ranger Data Asset's `PrimaryWeapon` soft ref to this asset. A Ranger whose soft ref is still empty still resolves RangerRifle by id, so the asset at the path above is enough to retire the fallback.

## Greybox actor tags (packaged builds)

`GetActorNameOrLabel` returns the outliner label only in editor builds. `MuteGreyboxProxyCollision` checks tags first:

| Tag | Result |
|-----|--------|
| `KodGround` | Keep collision (`KeptGround`). Wins if `KodGreybox` is also set. |
| `KodGreybox` | `NoCollision` on primitive components. |
| neither | Existing label rules: name or label contains `PROXY_`, except `PROXY_GROUND` which is kept. |

`KodHostileAnchor` is only the spawn anchor. It does not by itself mute or keep collision. The hostile label is still a `PROXY_*` prop, so it mutes by label in editor and needs `KodGreybox` as well in a packaged build.

Map edit (Director tooling, not a binary in git):

- `PROXY_GROUND` → tag `KodGround`
- every other `PROXY_*` → tag `KodGreybox`
- `PROXY_HOSTILE` also gets `KodHostileAnchor`

PIE log: `Slice0 PROXY collision muted Count=%d KeptGround=%d ByTag=%d ByLabel=%d`. `ByTag` is actors classified by `KodGround` or `KodGreybox`. `ByLabel` is actors classified by the label fallback. On the current untagged editor map that is `Count=23 KeptGround=1 ByTag=0 ByLabel=24` (23 muted props plus the kept floor). After the map is tagged, those same actors count as `ByTag` and `ByLabel` drops to 0.

## Walk-gate classes

| Role | Class |
|------|-------|
| Camera | `AKodRTSCameraPawn` |
| PC (base) | `AKodPlayerController` (select / marquee / mouse fallback) |
| PC (command path) | `AKodSlice0PlayerController` → `UKodCommandSubsystem` — `/Script/KingdomOfDust.KodSlice0PlayerController` |
| GameMode | `AKodSlice0GameMode` (bootstrap + optional smoke Ranger + passive hostile Ranger + PROXY collision mute, floor kept) — `/Script/KingdomOfDust.KodSlice0GameMode` |
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
- [ ] Left-click select. `AKodSlice0GameMode::StartPlay` sets `NoCollision` on primitive components of actors whose name or label contains `PROXY_`, except the floor whose name or label is `PROXY_GROUND` (L_Slice0 outliner label on `StaticMeshActor_0`). That floor stays BlockAll so the pawn capsule can stand on it and ECC_Pawn / ECC_Visibility still hit it. Other greybox stays placed and visible. Tags are checked first (`KodGreybox` mutes, `KodGround` keeps); actors with neither tag still use these label rules. PIE log: `Slice0 PROXY collision muted Count=23 KeptGround=1 ByTag=0 ByLabel=24` on the current untagged editor map (24 `PROXY_*` labels: 23 muted plus the kept floor). Muted actors must not appear in the click HitLog. The ray still walks ECC_Pawn past the unregistered floor and past sim actors whose `TeamId` is not the local player team, and selects the first local-team sim actor. A ray that only hits the floor clears selection. Drags shorter than `BoxSelectDragThresholdPx` (6) stay on this path. PIE log: `ClickSelectAtCursor LocalSelection=… Picked=… Hits=Actor(Pawn,sim=0|1,id=…,loc=…)`. An empty floor click logs the floor object name (`UEDPIE_0_StaticMeshActor_0` in PIE, `sim=0`) and `Picked=None`. `GetActorNameOrLabel` returns the outliner label only in editor builds; a packaged run sees `StaticMeshActor_*` and will not match these labels. The map needs tags (Director tooling, not this PR): `PROXY_GROUND` → `KodGround`; every other `PROXY_*` → `KodGreybox`; the `PROXY_HOSTILE` label also gets `KodHostileAnchor`. After that, the same mute log counts those actors as `ByTag` and `ByLabel=0`.
- [ ] Drag-select. LMB drag at or past 6px shows an orange screen rect on `AKodHUD` (`DrawRect` / `DrawLine`, engine white texture, no Content asset). On release, every local-team sim actor (not a foreign `TeamId`) whose viewport projection lies inside that rect is selected. The hostile test Ranger is left out, so it does not take the selection rim. A fresh drag replaces the selection; Left Shift adds (same as click). An empty box clears unless Shift is held. The rim overlay updates for the whole set. PIE: `DragSelect LocalSelection=… Additive=0|1 Box=x0,y0-x1,y1 Hits=Name(id=…,screen=…,…)|none` then `SelectionHighlight LocalSelection=… Meshes=… Overlay=1 Stencil=1`
- [ ] Right-click the floor → Move to the **first** cursor ImpactPoint (that ImpactPoint, not the actor pivot). `PROXY_GROUND` stays BlockAll, so the hit is the floor object name on ECC_Pawn (`sim=0`). Other `PROXY_*` meshes (CC, Dozer, crate, dock, pads, bounds, dirt, labels) are still `NoCollision` and are not that first Pawn hit. The ray walks past any remaining unregistered Pawn blocker the same way click-select does: a sim-registered actor on the ray that is not the current selection → Attack. Self / selection is not an attack target (Move at the first ImpactPoint instead). If ECC_Pawn hits nothing, `GetGroundHitUnderCursor` (ECC_Visibility) still hits the floor. PIE: `RMB Move LocalSelection=… Dest=x,y,z Hit=UEDPIE_0_StaticMeshActor_0 sim=0` then `Move Issued Sources=… Dest=…`, or `RMB Attack LocalSelection=… Target=… Id=…` then `Attack Issued Sources=… Target=… Id=…`. `RMB Miss none` means the floor was muted. `Attack Reject NonSim` means the pivot fallback did not run. A missed floor logs `Slice0 PROXY ground floor not kept (tag KodGround or label PROXY_GROUND). Floor traces will miss.`
- [ ] Right-click the red hostile → Attack hitscan (RangerRifle damage 12 / range 900). Select the smoke Ranger (team 0), RMB the red cube: `RMB Attack LocalSelection=… Target=… Id=…` then `Attack Issued Sources=… Target=… Id=…`. If `/Game/Warden/Data/RangerRifle` is missing, spawn also logs one `KodWeapon fallback RangerRifle Damage=12 Range=900 Cooldown=0.8125`. The hostile does not shoot back. The attacker faces the hostile (walks in first when farther than 900; the default anchors are inside 900 and fire in place). Each shot logs `KodSim Fire Attacker=… Target=… Tick=…` then `KodSim Hit Target=… Id=… HP=…` with remaining HP 108, 96, 84, 72, 60, 48, 36, 24, 12, 0. On the screen: orange muzzle flash and yellow tracer each shot, a white flash and a small jiggle on the red cube, a green bar over the selected Ranger, and a red bar over the hostile once it is damaged. The tenth hit logs `KodSim Kill Target=… Id=…` then `KodUnit Death <Name>`. The order clears in that step, the hostile unregisters, sinks into the floor over ~1.5 s, and the actor is destroyed. Clicking or drag-selecting the hostile does not add it or paint the rim. After the kill it is not selectable or attackable.
- [ ] Selected actors keep their own materials and gain a glow rim. The rim is the local translucent unlit Fresnel material `/Game/Warden/FX/Selection/M_SelectionRim.M_SelectionRim` (parameters RimColor, RimIntensity, RimExponent), loaded by soft object path and applied with `UMeshComponent::SetOverlayMaterial`. It is not committed as a `.uasset`. Material slots are not replaced. Custom depth stays on (`SetRenderCustomDepth(true)`, stencil `1`) so a later post-process outline can use it. Clearing or replacing the selection calls `SetOverlayMaterial(nullptr)` and restores the previous custom depth and stencil. If the rim fails to load, a warning is logged once and only the stencil is applied (no tint). `DefaultEngine.ini` sets `r.CustomDepth=3` so the stencil is written. PIE: `SelectionHighlight LocalSelection=… Meshes=… Overlay=1 Stencil=1` (`Overlay=0` when the material is missing).
- [ ] One Ranger walks; arrival stops in acceptance radius. While it moves, the body bobs (about 3.5 cm at 2.5 Hz) and the body yaw turns to face the move at 540 deg/s, easing level when it stops. Attacking turns yaw toward the hostile and holds that facing between shots. Spawn log, once per unit: `KodUnitLife <Name> Bob=3.5 Turn=540`
- [ ] Idle hash stable across frames when no orders. Bob, visual yaw, muzzle flash, tracer, hit jiggle, death sink, HP bars, and `TeamId` are not hashed. The hostile is a second entity, so the hash value differs from a one-Ranger world; with no orders it still stays stable across frames. A kill removes that entity from the hash; after the attacker clears its order the hash is stable again.
- [ ] HP from DA (Ranger 120) — no hardcoded HP in unit Tick
- [ ] Soft path or bootstrap resolves all six ids
- [ ] Ranger body uses `/Game/Warden/Characters/USA/Ranger/SM_Ranger_Body` when that static mesh is imported and set on `/Game/Warden/Data/Ranger`. The static body is auto-scaled to about 170 cm from mesh bounds (meter-import band-aid). Until the mesh is set, the unit stays the Engine cube (`KodUnitBody … Source=Cube`). Select / move / drag-select stay on the existing QueryOnly Pawn body. PIE: `KodUnitBody … Source=StaticMesh Mesh=SM_Ranger_Body` and a visible human-sized Ranger. The hostile test unit is the exception: it is forced to the cube (`KodUnitBody … Source=Cube`) with a red `BasicShapeMaterial` MID even when that static mesh loads.
- [ ] Hostile spawn. PIE: `Slice0 Hostile spawned Name=… Team=1 Loc=… Anchor=PROXY_HOSTILE` when the label or `KodHostileAnchor` tag is present, otherwise `Anchor=fallback` at `(1200, 600, 100)`. Red cube, no auto-attack. After it is killed it sinks and is destroyed (`KodUnit Death`).

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
