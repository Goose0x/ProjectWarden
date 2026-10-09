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

The USA basic infantry **display name is Marine**. The internal id stays `Ranger` (`DefinitionId`, `/Game/Warden/Data/Ranger`, `RangerRifle`, gameplay tag `Kod.Unit.Ranger`, log keys, class names). Bootstrap `UKodUnitDefinition::DisplayName` text is Marine (`NSLOCTEXT` key `Unit_Ranger`). On a loaded Ranger Data Asset, set the **DisplayName** property to Marine. Do not rename the asset.

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
| Skeletal mesh | Assign when the Marine skeletal asset exists. Empty keeps the static mesh, then the cube. |
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

Select collision is unchanged on the static path: after the mesh swap, `UnitMesh` is set again to the **Pawn** profile and **QueryOnly** (Visibility stays ignored by that profile). The static-mesh swap does not modify the capsule. Click-select and drag-select still use the same traces. The rim overlay and custom-depth stencil `1` still walk visible mesh components, so the body highlights the same way the cube did. The body's own materials stay visible.

## Marine skeletal body (no Content in git)

`UKodUnitDefinition::SkeletalMesh` wins over the static mesh when that asset loads. The team 1 hostile uses that same Ranger definition, so it gets the same skeletal mesh and AnimBP. It is a red-team Marine stand-in until real enemy units exist. Bootstrap leaves `SkeletalMesh` and `AnimClass` null, so catalog PIE is still the cube for both. `bForceCubeBody` remains on `SpawnRanger` for a caller that must keep the cube; the hostile spawn does not set it.

`MountResolvedSkeletalMesh` does not use the 170 cm static-mesh scale.

| Mount | Value |
|-------|--------|
| Relative yaw | `UKodUnitDefinition::SkeletalMeshYawOffset`, default **-90**. Bind pose faces +Y; -90 lines it up with actor forward. Actor yaw stays the sim facing |
| Relative scale | `1` |
| Height | **178.9 cm** at scale 1. Root is at the soles |
| Component offset | `(0, 0, -half-height)` with half-height **90**, so the soles touch the ground |
| Capsule | radius `34`, half-height `90`. Restored to the Character default `34` / `88` if the body falls back to the static mesh or cube |
| Skeleton | `SK_Marine_Skeleton`. Bone names match the mannequin set. It is a separate skeleton and is not shareable with `SK_Mannequin`. Do not retarget this mesh onto that skeleton |

Log: `KodUnitBody <name> Source=SkeletalMesh Yaw=-90 Scale=1 CapsuleHH=90 Radius=34`.

### Import layout

`/Game/Warden/Units/USA/Marine/` with folders `Mesh`, `Anims`, `Textures`, `Materials`, `Weapon`. Do not commit the binaries.

Clips in `Anims`: **Idle**, **Run**, **Walk**, **AimIdle**, **Fire**, **HitReact** (additive, Mesh Space, base **AimIdle frame 0**), **Death**, **Death_B**, **Acknowledge**.

Parent the carbine in `Weapon` to socket `weapon_r`.

`AnimClass` on the Ranger Data Asset is optional. **Empty, with any animation sequence set, is native mode:** `UKodUnitAnimInstance` (`/Script/KodUnits.KodUnitAnimInstance`) samples the clips in C++. No AnimBP asset is required. A set `AnimClass` (a Blueprint child of that class) still wins and plays its graph. Log: `KodUnit AnimMode=Native`, `AnimBP`, or `None`.

Native locomotion is SC2-style: Idle or Run, never a walk blend. `Speed` is the sim `MoveSpeed` while a Move is outside acceptance, or while an Attack is still seeking. It is not the interpolated actor position delta. Above about 20 cm/s the unit is **Run**, otherwise **Idle**. `WalkAnim` stays on the data asset and is not sampled. Run play rate is `SimSpeed / AuthoredRunSpeed` (450), clamped to `0.8–1.3`. If the sim speed is under `0.8 * 450`, one log per unit: `KodUnit RunRate=<raw> SimSpeed=<cm/s>`.

Blends: about `0.10` s into run, about `0.12` s back to idle or aim. Run, Idle, and AimIdle loop on `[0, (keys-1)/fps)` so a duplicate last key (Run is 21 keys for a 20-frame cycle) never samples. Run phase is kept across a stop shorter than `0.2` s. Root translation is stripped from the sampled pose. The skeletal path does not use the placeholder bob.

| Clip field | Behaviour |
|------------|-----------|
| `IdleAnim` / `RunAnim` | Idle, or Run while moving. `WalkAnim` is unused by the native blend |
| `AimIdleAnim` | While aiming and not moving |
| `FireAnim` | One-shot over the aim pose on each shot, `FirePlayRate` `0.952` (11 frames at 30 fps stretched to 0.35 s). Muzzle flash at frame 1 (`1/30` s of the clip, advanced by the play rate). A `UKodAnimNotify_Shot` on the sequence is used instead of frame 1 when it is present |
| `HitReactAnim` | Mesh-space additive (authored on AimIdle frame 0) for the length of the clip |
| `DeathAnim` / `DeathAnimB` | Even entity id plays Death (hold frame `DeathHoldFrame` **26**). Odd id plays Death_B (hold frame `DeathHoldFrameB` **30**). One shot, then hold. Sink starts `frame / 30 + 0.2` s after the hold frame, not at the clip end |

A missing sequence skips that layer. `None` (no clips and no AnimClass) keeps the cube or static mesh on the timed muzzle flash.

Fill these on `/Game/Warden/Data/Ranger` (`UKodUnitDefinition`, category **Animation**): `IdleAnim`, `WalkAnim`, `RunAnim`, `AimIdleAnim`, `FireAnim`, `HitReactAnim`, `DeathAnim`, `DeathAnimB`, `DeathHoldFrame`, `DeathHoldFrameB`, `WeaponMesh`, `WeaponSocket` (default `weapon_r`). `SkeletalMeshYawOffset` stays **-90**.

`WeaponMesh` is a static mesh attached to `WeaponSocket` with a zero offset, no collision. It takes the same selection rim as the body and the same `TeamColor` MID when the material has that param. Log: `KodUnit Weapon attached=<mesh> socket=<name>` (`attached=None` when the mesh is empty). Muzzle search order: `SOCKET_Muzzle` on that weapon mesh, then `SOCKET_Muzzle` on the skeleton, then `weapon_r`, then the body-front flash.

### AnimBP

Not required. Native mode plays the sequences above with no state machine. Use this only when an AnimBP asset exists. Parent class: `UKodUnitAnimInstance`.

The instance reads sim state. It does not write orders, pose, or the idle hash.

| Pin | Meaning |
|-----|---------|
| `Speed` | Sim `MoveSpeed` while moving on a Move or Attack seek, else `0`. Not a frame delta |
| `AuthoredRunSpeed` | `450`. Stride `300` cm/cycle. Shown run rate is clamped to `0.8–1.3` |
| `AuthoredWalkSpeed` | `150`. `WalkPlayRate = Speed / AuthoredWalkSpeed` |
| `FirePlayRate` | `0.952`. Fire is 11 frames at 30 fps, `(11-1)/30` s, and `0.952` makes that `0.35` s |
| `bIsAiming` | Sim order is Attack and the target is still alive |
| `bIsDead` | Death presentation has started |
| `bDeathVariantB` | Odd entity id plays **Death_B**. Even id plays **Death** |
| `bHitReact` | True for one anim update after `OnUnitHit`, then cleared |
| `FireCounter` | Increments on each `OnUnitFired` |
| `OnFire` | Broadcast with that shot |
| `DeathAnim` / `DeathAnimB` | The two death clips. Both empty: sink starts immediately |
| `DeathHoldFrame` / `DeathHoldFrameB` | `26` and `30` at `AuthoredFrameRate` `30`. Sink starts `frame/30 + 0.2` s, not at the clip end |

State machine:

1. **Idle / Run** — above about 20 cm/s play Run at `RunPlayRate` (clamped `0.8–1.3`). Otherwise Idle. Do not blend `WalkAnim` in. `WalkPlayRate` remains for a later clip.
2. **AimIdle / Fire** — when `bIsAiming`, hold AimIdle. Enter Fire when `FireCounter` changes and play it at `FirePlayRate`. Put `UKodAnimNotify_Shot` (`/Script/KodUnits.KodAnimNotify_Shot`, notify name **Kod Shot**) on **frame 1** of Fire.
3. **HitReact** — additive, **Mesh Space**, on an **AimIdle frame 0** base. Trigger from `bHitReact`.
4. **Death** — `bDeathVariantB` selects Death_B, otherwise Death. Hold starts at frame 30 or frame 26. The actor stays until `0.2` s after that frame, then the `1.5` s sink. The clip can keep holding its last frame. No death clip assigned keeps the immediate sink.

### Sockets

| Socket | Use |
|--------|-----|
| `weapon_r` | Weapon attach. Muzzle fallback when `SOCKET_Muzzle` is missing |
| `SOCKET_Muzzle` | Shot notify flash origin. Searched on every skeletal component (weapon mesh included) |
| `SOCKET_ShellEject` | Shell eject, for the AnimBP / FX later |
| `SOCKET_LeftHandGrip` | Left hand on the weapon |
| `FX_Foot_l` | Left foot |
| `FX_Foot_r` | Right foot |
| `FX_Chest` | Chest |

The notify drives the existing point light, sphere, cone, and yellow tracer from `SOCKET_Muzzle`, else `weapon_r`. No notify, no socket, a non-skeletal body, or the C++ anim class with no AnimBP uses the timed body-front flash (about `0.06` s light, `0.08` s tracer). An AnimBP that never notifies falls back to that flash after `0.5` s so the shot is still visible. The cube and static paths do not wait.

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
| `TurnRateDegreesPerSecond` | 900 | `RInterpConstantTo` on body yaw. Skeletal mesh uses this on top of `SkeletalMeshYawOffset`. Cube/static use `UnitMesh` |

The amplitude is multiplied by `(mesh local height × RelativeScale3D.Z) / 170`. The Engine cube (100 cm at scale 1.7) and `MountResolvedStaticMesh`'s ~170 cm Ranger both stay near `BobAmplitudeCm`. Sine weight eases to 0 at rest and the mesh relative location is put back on the captured attach point.

Visual yaw chases sim `YawDegrees` (movement facing, and the attack target while seeking or holding range). On a skeletal body that yaw is `SkeletalMeshYawOffset` plus the short-arc catch-up, on the skeletal mesh relative rotation. Cube and static bodies use `UnitMesh` relative yaw. It does not call `SetActorRotation` and it does not write the sim value. The actor yaw stays the interpolated sim facing. A Move snaps sim yaw to the destination immediately; an Attack snaps it toward the target, and the attack step keeps that facing while firing.

Once per unit at BeginPlay: `KodUnitLife <Name> Bob=3.5 Turn=900` (the tunables, not the height-scaled amplitude). The skeletal mesh does not bob. Sim yaw is set to the travel direction when a Move is issued, and toward the target when an Attack is issued or firing. Presentation yaw takes the short arc (`FindDeltaAngleDegrees`) and the body catches up at 900 deg/s. `kod.AnimDebug 1` logs `KodUnit AnimDebug` every 0.25 s: Speed, Idle/Run/Aim weights, run time, and run play rate.

## Hostile test target

After PROXY mute and the smoke Ranger, `AKodSlice0GameMode::SpawnHostileTestTarget` spawns one more Ranger from the same definition on **team 1**. Spawn does not issue an order and there is no AI controller. The sim auto-acquires for it (and for the team 0 Marine) on the next idle step.

Placement: actor tag `KodHostileAnchor` first, else an actor whose name or label is `PROXY_HOSTILE` (PIE object names ending in `_PROXY_HOSTILE` count). XY comes from that actor; Z is the smoke Ranger Z (`SmokeRangerOffset.Z`, default 100) so the label's text height is not used. If neither tag nor label exists, spawn at `(1200, 600, 100)`.

Visual: the hostile mounts the same body as the player Marine. When `/Game/Warden/Data/Ranger` has a skeletal mesh, both are that mesh (`Body=Skeletal`) and share `AnimClass`. If the skeletal soft ref fails, the body falls through to the static mesh, then the Engine cube, so PIE still runs before the art import (`Body=Cube`).

Team colour is the strong base on the large armour areas, per player. Faction trim stays on the material and is not recolored. `ApplyTeamColor` sets the vector param **TeamColor** on a dynamic MID; the TeamMask texture stays on the asset. Defaults, both editable on `AKodUnit`: team 0 USA sand `#D2C4B1` (`FriendlyTeamColor`), team 1 `#B3261E` (`HostileTeamColor`). **Palette is pending Art.** If no slot has `TeamColor`, team 1 falls back to the old `Color` / `BaseColor` tint (BasicShapeMaterial on the cube). Team 0 stays untinted in that case. The selection rim is still an overlay, not a slot replace.

Click-select and drag-select only take sim actors whose `TeamId` matches `AKodPlayerState::TeamId` (default 0). The hostile is walked past and gets no rim. RMB is unchanged: a sim actor that is not the current selection, including team 1, logs `RMB Attack` and then `Attack Issued`.

PIE: `Slice0 Hostile spawned Name=<name> Team=1 Loc=<x,y,z> Anchor=PROXY_HOSTILE Body=Skeletal` when the skeletal mesh loads, or `Body=Cube` when it does not. `Anchor=fallback` is the `(1200, 600, 100)` placement. The hostile is a red-team Marine stand-in until real enemy units exist.

HP stays whatever the Ranger definition says. Bootstrap catalog `MaxHealth` is 120. A loaded `/Game/Warden/Data/Ranger` wins: the Director's PIE asset is **100** (first `KodSim Hit` remaining **88**, nine hits of 12 to reach 0). Do not hardcode HP in the unit.

Smoke spawn `(400, 0, 100)` to the default hostile anchor `(700, 200, 100)` is about 360 cm, inside range 900, so both units turn and fire in place. A target farther than 900 is seeked until it is in range, then the attacker stops and fires.

## Auto-acquire (both teams)

`StepSim` handles Idle units (`Order == None`) with `WeaponDamage > 0`. An explicit **Move** is not this path: the unit does not acquire until that move arrives and the order clears. The next idle step may then acquire.

Each idle step, in entity-id order:

1. If `RetaliateTarget` is set (this unit was hit while Idle), attack that attacker even when they are outside `WeaponRange`. Log `Reason=Retaliate`. The seek is the same `StepEntityAttack` path.
2. Otherwise pick the nearest enemy (`TeamId` differs, HP > 0) inside `WeaponRange`. Ties go to the lowest entity id. Log `Reason=InRange`.
3. Log once, when the order is issued: `KodSim AutoAcquire Unit=<actor name> Target=<actor name> Reason=InRange|Retaliate`. Then run `StepEntityAttack` the same step.

A unit that already has Attack keeps that target. A hit on a moving or already-attacking unit does not retarget. `RetaliateTarget` is not part of `ComputeIdleHash`. Explicit Move, Attack, and Stop clear it.

Default spawn order is smoke Marine first (id 1, usually `KodUnit_0`) then the hostile (id 2, usually `KodUnit_1`). The lower id acquires `InRange` and shoots first. That hit lands while the other unit is still Idle, so the other logs `Retaliate` and shoots back the same step. Later shots do not log AutoAcquire again. Because the lower id shoots first, it lands the killing blow; the other unit has fired one fewer shot. The corpse sinks, drops out of selection if it was selected, and is unregistered. Either unit can die: team 0 uses the same death presentation and the green bar while it is damaged.

Muzzle flash and tracer are per unit. The hostile is the same Marine body, tinted team 1, so it plays the same fire, hit react, and death clips once the AnimBP is assigned. Before that mesh loads it is the red cube and still uses the timed muzzle flash. HP bars: green over team 0, red over team 1, hidden at full HP unless that unit is selected, visible on both once either has been hit.

### Fight test (expected PIE)

No RMB required. After both spawns the next sim steps look like this (actor names follow spawn order):

```
KodSim AutoAcquire Unit=KodUnit_0 Target=KodUnit_1 Reason=InRange
KodSim Fire Attacker=KodUnit_0 Target=KodUnit_1 Tick=<n>
KodSim Hit Target=KodUnit_1 Id=2 HP=<MaxHealth-12>
KodSim AutoAcquire Unit=KodUnit_1 Target=KodUnit_0 Reason=Retaliate
KodSim Fire Attacker=KodUnit_1 Target=KodUnit_0 Tick=<n>
KodSim Hit Target=KodUnit_0 Id=1 HP=<MaxHealth-12>
```

Those Fire/Hit pairs repeat until one side reaches 0. With the loaded MaxHealth 100 that is nine player shots (88, 76, 64, 52, 40, 28, 16, 4, 0) and eight hostile shots. The killing step:

```
KodSim Fire Attacker=KodUnit_0 Target=KodUnit_1 Tick=<n>
KodSim Hit Target=KodUnit_1 Id=2 HP=0
KodSim Kill Target=KodUnit_1 Id=2
KodUnit Death KodUnit_1
```

Bootstrap-only (no Ranger asset) uses MaxHealth 120, so the first remaining HP is 108 and the kill is the tenth player shot. The hostile's muzzle flash, yellow tracer, white hit flash on the Marine, and red jiggle on the hostile all play. Both bars show after the first exchange. A Move order on the Marine clears its attack, so the hostile keeps firing and can be the one that lands the kill. If the Marine reaches 0 it sinks, logs `KodUnit Death`, leaves the selection, and its green bar was visible while it was damaged.

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
| GameMode | `AKodSlice0GameMode` (bootstrap + optional smoke Marine + hostile Marine that auto-acquires + PROXY collision mute, floor kept) — `/Script/KingdomOfDust.KodSlice0GameMode` |
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
- [ ] Fight. Default spawns are inside range 900, so both idle Marines auto-acquire with no RMB: `KodSim AutoAcquire Unit=… Target=… Reason=InRange` on the lower id, then Fire/Hit, then `Reason=Retaliate` on the unit that was hit while still idle, then its Fire/Hit. Both sides keep firing. HP is the Ranger Data Asset MaxHealth minus 12 per hit (loaded asset 100 → first remaining 88, nine hits to 0; bootstrap catalog 120 → first remaining 108). The lower id lands the kill: `KodSim Kill` then `KodUnit Death`. Orange muzzle flash and yellow tracer play on **both** units. Green bar on team 0 and red bar on team 1 once damaged. The dead unit sinks ~1.5 s, drops out of selection, and is not selectable. A Move order does not acquire until the unit arrives. RMB Attack still works and logs `RMB Attack` / `Attack Issued`.
- [ ] Selected actors keep their own materials and gain a glow rim. The rim is the local translucent unlit Fresnel material `/Game/Warden/FX/Selection/M_SelectionRim.M_SelectionRim` (parameters RimColor, RimIntensity, RimExponent), loaded by soft object path and applied with `UMeshComponent::SetOverlayMaterial`. It is not committed as a `.uasset`. Material slots are not replaced. Custom depth stays on (`SetRenderCustomDepth(true)`, stencil `1`) so a later post-process outline can use it. Clearing or replacing the selection calls `SetOverlayMaterial(nullptr)` and restores the previous custom depth and stencil. If the rim fails to load, a warning is logged once and only the stencil is applied (no tint). `DefaultEngine.ini` sets `r.CustomDepth=3` so the stencil is written. PIE: `SelectionHighlight LocalSelection=… Meshes=… Overlay=1 Stencil=1` (`Overlay=0` when the material is missing).
- [ ] One Marine runs; arrival stops in acceptance radius. A Move order does not auto-acquire until that arrival. Native locomotion is Idle or Run (not a walk blend), facing the travel direction at 900 deg/s, and the skeletal mesh does not bob. Firing faces the target. Spawn log, once per unit: `KodUnitLife <Name> Bob=3.5 Turn=900`. The actor name in logs stays `KodUnit_*`; the player-facing display name is Marine.
- [ ] Idle hash stable across frames when no orders. Bob, visual yaw, muzzle flash, tracer, hit jiggle, death sink, HP bars, `TeamId`, and `RetaliateTarget` are not hashed. The hostile is a second entity, so the hash value differs from a one-unit world. Auto-acquire gives both units an Attack order, so the world is not idle during the fight. After the kill the survivor's order is clear and the hash is stable again.
- [ ] HP from DA (Ranger 120) — no hardcoded HP in unit Tick
- [ ] Soft path or bootstrap resolves all six ids
- [ ] Ranger body uses `/Game/Warden/Characters/USA/Ranger/SM_Ranger_Body` when that static mesh is imported and set on `/Game/Warden/Data/Ranger`. The static body is auto-scaled to about 170 cm from mesh bounds (meter-import band-aid). The delivered skeletal mesh (178.9 cm, `SK_Marine_Skeleton`, import under `/Game/Warden/Units/USA/Marine/`) wins for **both** the player and the hostile: `SkeletalMeshYawOffset` default -90, scale 1, soles on a 34×90 capsule, log `KodUnitBody … Source=SkeletalMesh Yaw=-90 Scale=1 CapsuleHH=90 Radius=34`. Team 0 is sand `#D2C4B1` and team 1 is `#B3261E` when the material has `TeamColor` (palette pending Art). Until the skeletal mesh is set, both stay the Engine cube (`Body=Cube`); the hostile cube still tints red. Select / move / drag-select stay on the existing QueryOnly Pawn body.
- [ ] Hostile spawn. PIE: `Slice0 Hostile spawned Name=… Team=1 Loc=… Anchor=PROXY_HOSTILE Body=Skeletal` when the label or `KodHostileAnchor` tag is present and the skeletal mesh loads, otherwise `Body=Cube`. No anchor logs `Anchor=fallback` at `(1200, 600, 100)`. Same Ranger definition as the player, red team colour, auto-acquire, death / fire / hit react when the AnimBP is assigned. The team 0 Marine can die the same way.

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
