# Kingdom of Dust (Warden): Core-loop UE architecture guide, v1

**Owner:** Unreal Engine SME (guidance + gate review) · **Implementer:** Lead Software Dev · **Date:** 2026-10-11 PT
**Scope:** Director build push. The playable loop is gather → build → train → abilities → army.
**Grounded in:** `Goose0x/ProjectWarden` `main` @ `a7ca04d` (2026-10-09 09:03 PT). Anything marked *[assumed]* was not verified in code.
**Status:** PROPOSED. It becomes binding once Lead Dev ACKs. Calls marked **CALL** are SME decisions. Calls marked **ASK** need someone else.

---

## 0. What already exists (read this first)

More of the loop is already on `main` than the brief implies. Build on these pieces and don't duplicate them:

| Area | Existing (verified) | State |
|---|---|---|
| Sim authority | `UKodSimSubsystem : UTickableWorldSubsystem` (KodCore). `SimHz=16`, `MaxCatchUpSteps=4`. `FKodSimEntityState` holds pose, HP, order, weapon, team and gather fields. `ComputeIdleHash` | Solid base. Every system below hangs off it. |
| Commands | `FKodCommand` (KodNet). Types: `Move/Attack/Stop/Build/CastPower/Gather/SetRally`. Has `bQueued`, `IssuerPlayerId` and `SourceEntities`. `UKodCommandSubsystem` drains it every frame | `bQueued` is ignored. Drains on the frame, not the sim tick. `ExecuteBuild` is a stub on `UKodBuildQueueComponent`. |
| Gather | Sim gather FSM (`EKodGatherPhase`, cargo, harvest/stall ticks). `Banks` keyed by team (Jadeite/Luminene). `RegisterResourceNode` and `RegisterDropOff` | Works. Keep it. |
| Economy (2nd) | `UKodResourceWallet` (KodEconomy): replicated `Cash/Oil/Supply/SupplyCap`. Defs use `BuildCostCash` | **Conflicts** with the sim `Banks`. See §3. |
| Production | `UKodBuildQueueComponent`: **1 slot**, float seconds on `TickComponent`, never spends | Must move into the sim. See §5. |
| Input/select | `AKodPlayerController` (KodCore) holds `PlayerTick` + `WasInputKeyJustPressed` greybox. Has `DefaultMappingContext/SelectAction/CommandMoveAction` soft refs, but the IMC is added and nothing is bound. It has its own `LocalSelection`. `AKodSlice0PlayerController` routes through `UKodCommandSubsystem` | Two selection stores. See §1. |
| UI-side | `UKodSelectionManager` and `UKodControlGroupStore` (0–9) as `ULocalPlayerSubsystem`. `UKodHotkeyRouter`, `UKodCommandCardWidget` | Exists, but is not the single source of truth. |
| GAS | Plugin on. `AKodUnit` implements `IAbilitySystemInterface`, has a `UKodAbilitySystemComponent` per unit and a `UKodCombatAttributeSet` (Health/MaxHealth). `KodGenerals` has `UKodGeneralPowerComponent` and `GA_KodPower_DustStorm` | Second HP store. See §6. |
| Units | `AKodUnit : ACharacter`. CMC is forced to `MOVE_None`, `MaxWalkSpeed=0`. Mesh is QueryOnly Pawn. `ApplyTeamColor()` uses a per-actor **MID** | See §2 and §7. |
| Data | `UKodUnitDefinition/BuildingDefinition/WeaponDefinition/FactionDefinition/ResourceNodeDefinition` (PrimaryDataAsset, bare ids), `UKodDataCatalog`, `UKodSlice0Bootstrap` fallback, `KodDataValidation` (editor) | Good. CSV overrides are added on top (§8). |

Module layout (verified): `KodCore` (sim, game framework) ← `KodUnits` ← `KodEconomy`, `KodGenerals`, `KodAI`, `KodNet`, `KodUI`, with `KodEditor` and the `KingdomOfDust` game module (Slice0 GM/PC). There is **no `RTSCore` module**. `KodCore` is the sim module. **CALL:** add no new modules this push. Sim code goes into `KodCore/Sim/`. **Pitfall:** `KodCore` cannot see `KodUnits` definitions, so the sim takes plain structs (`FKodUnitSimSpec`) that `KodUnits` fills from the DataAssets. This is the same pattern as `FKodResourceNodeSpec` today.

Global sim rules for this push:
- All gameplay state lives in the sim. Actors only interpolate and present it.
- Only `FKodCommand` mutates the sim.
- All durations are integer ticks.
- Iterate entities in **ascending id order**. Don't rely on `TMap` iteration order.

---

## 1. Camera, selection, commands

**Approach.** Use Enhanced Input for intent capture, `UKodSelectionManager` as the single selection store, and `FKodCommand` as the only order path. The sim applies commands **at the start of the next sim tick**, stamped with `ExecuteTick`.

**Add / change**
- Assets in `Content/Core/Input/` (the README already names them): `IMC_KodRTS`, `IA_Select`, `IA_CommandMove`, `IA_CommandAttack`, `IA_Stop`, `IA_ControlGroup_0..9`, `IA_CameraPan`, `IA_CameraZoom`, `IA_ModQueue` (Shift), `IA_ModGroupAssign` (Ctrl), `IA_ModAdd` (Shift as well; used for selection).
- `AKodPlayerController::SetupInputComponent` binds these on a `UEnhancedInputComponent`. Keep the `PlayerTick` poll behind a CVar `Kod.Input.LegacyPoll 1` for one week, then delete it.
- **CALL:** delete `AKodPlayerController::LocalSelection` and read and write `UKodSelectionManager` instead. Keep the team filter (`TeamId == AKodPlayerState::TeamId`) inside the manager.
- `FKodCommand` gets `int32 ExecuteTick` and `FIntPoint TargetCell`. Split `Build` into `PlaceStructure` and `Train`, and add `CancelTrain` and `UseAbility`. Keep `bQueued`.
- Sim: add `TArray<FKodSimOrder, TInlineAllocator<4>> OrderQueue` (max 8) to `FKodSimEntityState`. A non-queued command replaces the queue. A queued one appends. Finishing an order pops the next.
- `UKodCommandSubsystem` stops executing commands in its own `Tick`. `UKodSimSubsystem::StepSim` pulls the commands due this tick at the top of each step. Process them sorted by `(ExecuteTick, IssuerPlayerId, CommandId)`.
- Control groups: wire `IA_ControlGroup_N` to the existing `UKodControlGroupStore`. Ctrl assigns, Shift adds, double-tap centres the camera on the group (`AKodRTSCameraPawn`).

**Data flow.** IA event → PC resolves cursor (existing `ECC_Pawn` walk, then `ECC_Visibility` for ground) → `FKodCommand{SourceEntities = Selection, bQueued = Shift}` → `UKodCommandSubsystem::Enqueue` → the sim drains it at tick N → `OrderQueue` → presentation reads the state.

**Stub first.** Bind `IA_Select`, `IA_CommandMove`, `IA_ModQueue` and the control groups. A shift-queued move chain of 3 waypoints must work and must not change the idle-hash rules.

**Pitfalls.**
- Enhanced Input with `GameAndUI` plus CommonUI can eat clicks. Keep `CaptureDuringMouseDown` and check that CommonUI input config doesn't swallow LMB over the viewport.
- Shift is both "add to selection" (LMB) and "queue" (RMB). That is fine, but the action is chosen by the mouse button, not by the modifier.
- Never let UI widgets call `Sim->IssueMove` directly. The existing `AKodPlayerController::IssueMoveToSelection` default calls the sim directly, so route it through commands too.
- Floats in the sim make it "deterministic-ish" on one binary or platform. That is enough for offline play and replays. Lockstep MP needs fixed-point later; do not do that this push.

---

## 2. Movement and pathing at scale (hundreds of units)

**Approach (CALL).** The sim owns movement on the 16 Hz tick, as it does today. Add (a) a uniform spatial hash for separation and target queries, (b) a static occupancy grid driven by building footprints (§4), and (c) grid pathing on that same grid. **Do not use NavMesh**, not even as a query; the grid replaces it. Do not use the CMC.

**Incremental, Slice-safe steps** (each step keeps the idle hash stable and is shippable on its own):
1. **Now:** keep straight-line seek. Add a `FKodSpatialHash` with 200 uu cells, rebuilt every tick from sim positions. Add **separation**: push idle and moving units apart by `CollisionRadius` from the DA, capped per tick. Add **arrival spreading**: group moves get per-unit offsets in a formation disc around the target, so 20 units don't fight over one point.
2. **Week 1 end:** `UKodPathGridSubsystem` (KodCore) holds a passability bitmap at the 200 uu build grid, optionally subdivided to 100 uu for units. Buildings stamp into it. Paths use A* on the grid plus string-pulling, **one path per group move** (a leader path that the followers offset from).
3. **Post-push:** flow fields per goal cell (cached, LRU), used for large group moves. Use 8 × 8 sectors for hierarchical A* when maps grow.
4. **Presentation at scale:** at more than ~150 units, `AKodUnit : ACharacter` costs too much even with `MOVE_None` (capsule, CMC tick, skeletal tick). Path: first set `CMC->SetComponentTickEnabled(false)` and use skeletal anim budget/URO. Later, add an `AKodUnitPawn : APawn` with no CMC (capsule-less, mesh only). Mass/ISM crowds come later, and only for presentation. Keep the sim API unchanged so these swaps are invisible to gameplay.

**Data flow.** Commands → `OrderQueue` → `StepEntityMove` (path waypoint → seek → separation → clamp to passable cell) → `Position/Yaw` → `SyncActorPresentation(alpha)`.

**Pitfalls.**
- Separation must not be computed from `TMap` order, or two units can end up resolving differently. Iterate sorted ids and use a two-pass approach (compute all pushes, then apply).
- Separation pushing units into building footprints: clamp after the push, against the occupancy grid.
- Air units (later) skip the grid. Plan an `EKodLocomotion` flag now.
- Do not reintroduce `AIMoveTo`. `KodAI` depends on `NavigationSystem`, so it stays out of match state.

---

## 3. Resource gathering

**Approach.** Keep the existing sim gather FSM. The worker loop (walk → harvest ticks → carry → drop-off → repeat), node depletion, trickle and the stall/retarget logic are already in the sim and hashed. That is the correct design.

**CALL: one ledger.** The sim `Banks` (per team, integer, in the idle hash) is **the** ledger. `UKodResourceWallet` (Cash/Oil, replicated) is retired as an authority. Either delete it or turn it into a read-only UI mirror refreshed from the sim. Move `Supply/SupplyCap` into the sim team state, as `FKodSimTeamState { Bank, SupplyUsed, SupplyCap }`, and keep the `KodSupplyHardCap = 200` constant.

**Change.** Replace `int32 BuildCostCash` with `FKodResourceCost Cost` on `UKodUnitDefinition` and `UKodBuildingDefinition` (Jadeite/Luminene), and add `int32 SupplyCost` (units) and `int32 SupplyProvided` (CC/depot).
- **ASK Strategy:** confirm the currency names in code are Jadeite/Luminene, and that "Cash/Oil" is legacy wording.
- Drop-offs: the CC already registers through `RegisterDropOff`. Any building whose definition has `bIsDropOff` registers on construction completion.

**Stub first.** One Dozer gathers Jadeite into the team-0 bank, the HUD readout shows it, and it reads correctly through the existing `KodResourceReadoutWidget`.

**Pitfalls.**
- Two ledgers mean a guaranteed desync: UI shows one value while spending reads the other. Resolve this before production work.
- Worker saturation: in SC2 the worker spreads to free patches, and that drives the economy feel. The sim needs per-node occupancy (one harvester at a time, the others wait or retarget), or 12 workers stack on one crystal. Small change, high polish value.

---

## 4. Build placement

**Approach.** A presentation ghost snaps to the locked grid: GridCell 200 uu, fixed facing, footprints from each building's `FootprintX` / `FootprintY` (`WARDEN_BUILD_GRID_LOCKED_20261002.md`). The 2026-10-17 lock is CC 5×5 (1000 uu), Barracks 3×3 (600), Bunker 3×3 (600), Supply Depot 2×2 (400). Addons stay 2×2 and socket to the parent. Validation runs **twice**: on the client for ghost colour, and in the sim on command execution (authoritative).

**Add**
- `UKodBuildGridSubsystem` (KodCore/Sim): occupancy bitmap keyed by cell. `CanPlace(FootprintCells, OriginCell, TeamId)`, `Stamp`, `Unstamp`. It shares its bitmap with `UKodPathGridSubsystem`, which can be the same subsystem.
- `UKodBuildingDefinition` adds:
  - `FIntPoint FootprintCells`
  - `bool bIsAddon`
  - `TArray<FName> AddonParentIds`
  - `FIntPoint AddonSocketOffset`
  - `bool bIsDropOff`
  - `int32 SupplyProvided`
  - `TArray<FName> BuilderUnitIds`
- `AKodPlacementGhost` (KodUnits, presentation only) uses the def's proxy mesh with a translucent green/red MID and a per-cell decal or grid overlay.
- `FKodCommand::PlaceStructure{SourceEntities = [Dozer], PayloadName = BuildingId, TargetCell}`. Sim flow: validate → **reserve cells** and spend the cost → the Dozer walks to the footprint edge → spawn the building entity in `UnderConstruction`, with HP rising from 10% to `MaxHealth` over `BuildTicks` → `Complete`.
- Sim building state: `ConstructionTicksRemaining` and `EKodBuildingPhase {Reserved, UnderConstruction, Complete}`. These are hashed.

**Stub first.** Place a Barracks with the Dozer. Show a red ghost on overlap, and reject at sim level. Construction completes after `320` ticks (20 s), then the Barracks can train.

**Pitfalls.**
- Grid origin: one world origin per map (an `AKodMapGridOrigin` actor or world settings), cells of 200 uu. Pivot snap is per axis and depends on that axis's footprint, not on a global even-size assumption. Even N snaps the pivot to a grid line (a cell corner of the footprint). Odd N snaps it to a cell centre, 100 uu off the grid line. The occupied cells are the half-open N×N box centred on that pivot, so the far edge is not an extra cell. `CellOf` on an even pivot is the wrong origin: that point sits on the boundary and floors into the next cell.
- Rule decision (**ASK Strategy**): the C&C model has the Dozer stay and build, while the SC2 SCV model has it stay but be cancellable. Recommendation: the Dozer stays, progress pauses if it leaves, and another Dozer can resume. Cancel refunds 75%.
- Units standing in the footprint: block placement on enemy units, and push friendly units out on stamp.
- Addons ignore the free-placement path. They are only valid at `Parent.OriginCell + AddonSocketOffset`.

---

## 5. Production queues

**Approach (CALL).** Production state lives in the sim. `UKodBuildQueueComponent` (float seconds, `TickComponent`, one slot, no spend) becomes a thin presentation reader or is deleted.

**Add.** `FKodProductionQueue` in the sim building state:
- `TArray<FName, TInlineAllocator<5>> Slots` (max **5**)
- `int32 ActiveTicksRemaining`
- `bool bSupplyBlocked`
- `FVector RallyPoint`
- `FKodEntityId RallyTarget`

**Rules (SC2 parity; Strategy can override):**
- **Cost** is spent at enqueue and refunded **100%** on cancel of any slot.
- **Supply** is checked when a slot becomes *active*. If it doesn't fit, the queue pauses with `bSupplyBlocked`, and the UI shows "Not enough supply".
- `SupplyCap = min(Σ SupplyProvided of completed buildings, 200)`.
- On completion: spawn at a free cell next to the footprint (spiral search on the occupancy grid), then issue a queued move to the rally. If the rally is on a resource node, a worker gets a Gather order instead.
- Commands: `Train(BuildingId, UnitId)`, `CancelTrain(BuildingId, SlotIndex)` (LIFO by default), and `SetRally` (already exists).

**Data flow.** Command card button → `Train` → sim validates tech/affordability → slot is added → it ticks → `SpawnUnitFromDef` (KodUnits spawns the actor, then `RegisterEntity`/`ConfigureEntity` from the DA) → rally order.

**Stub first.** The CC trains a Dozer (128 ticks) and the Barracks trains a Ranger/Marine (80 ticks). Cover 5-slot queue, cancel and refund, supply block and rally.

**Pitfalls.**
- Actor spawn has to be deferred to *after* the sim step: the sim records "spawn requests" and presentation spawns them. Otherwise `RegisterEntity` happens inside the iteration loop.
- The entity id must be allocated by the sim at completion, not by actor `BeginPlay`. That keeps ids deterministic.
- `ExecuteBuild` uses `LoadObject` on `PayloadName`. Replace it with a `UKodDataCatalog` lookup by bare id.

---

## 6. Abilities: GAS or custom?

**CALL: do not run unit abilities through GAS for the sim.** Build a small sim-side ability system. Keep GAS *only* as an optional presentation and authoring layer for General powers, and route its effect through a sim command.

**Why.**
- GAS is built around actor-replicated, frame-ticked, client-predicted gameplay. That fights a fixed 16 Hz integer-tick sim, a single command stream and idle-hash determinism.
- A full ASC per unit (hundreds of units) adds memory, tick and replication cost for no benefit.
- `UKodCombatAttributeSet::Health` already duplicates `FKodSimEntityState::Health`, which breaks "HP from DataAssets only".
- What we lose: GameplayEffects stacking and GameplayCues. We'll replace those with a small effect list and Niagara hooks.

**Minimal setup.**
- `UKodAbilityDefinition` (PrimaryDataAsset, bare id), with:
  - `FGameplayTag AbilityTag`
  - `EKodAbilityTarget {None, Point, Unit}`
  - `FKodResourceCost Cost`
  - `int32 HpCost`
  - `int32 CooldownTicks`
  - `int32 DurationTicks`
  - `float Range`
  - a list of `FKodSimModifier {Stat, Op, Value}` (e.g. MoveSpeed ×1.5, CooldownRemaining ×0.5)
  - soft refs to Niagara/sound for presentation
- `UKodUnitDefinition` gets `TArray<TSoftObjectPtr<UKodAbilityDefinition>> Abilities`.
- Sim per entity: `TArray<FKodAbilityRuntime, TInlineAllocator<4>>` (cooldown and active-duration ticks) plus the active modifiers. Stats are recomputed each tick as base DA stats × active modifiers.
- Command: `UseAbility{SourceEntities, PayloadName = AbilityId, TargetEntity/Location}`. Smart-cast picks the subset of the selection that is off cooldown and closest, as in SC2.
- Presentation subscribes to `OnAbilityActivated(Entity, AbilityId, Tick)`, the same pattern as `OnUnitFired`.
- General powers: `GA_KodPower_DustStorm` may keep GAS for targeting UI and cues. Its `ActivateAbility` must only `Enqueue` a `CastPower` command. Damage and area effects resolve in the sim.
- **Gate:** remove `UKodAbilitySystemComponent` and `UKodCombatAttributeSet` from `AKodUnit`, or leave them unused and documented as dead. Do not read Health from them.

**Stub first.** One self-buff ability (Marine stim-type: −HP, +move and attack speed for 11 s = 176 ticks, cooldown in ticks) on the hotkey.

---

## 7. Team-colour material mask

**Approach.**
- One master material function, `MF_KodTeamTint`, used inside `M_KodUnitMaster` and `M_KodBuildingMaster`. Material instances per asset only carry textures.
- Team colour comes in through **Custom Primitive Data** (CPD), not per-actor MIDs. CPD keeps one material instance per asset. Static, ISM and Nanite buildings keep batching and instancing, and we skip per-actor MID creation.
- Replace `AKodUnit::ApplyTeamColor()` (MID `SetVectorParameterValue`) with `SetCustomPrimitiveDataVector4(0, …)` on every visible primitive (body, weapon, attachments).

**Material function (inputs → BaseColor):**
```
A   = FactionAlbedo.rgb            // T_<Asset>_BC, sRGB on → sampled linear
TM  = T_<Asset>_TM.rg              // sRGB OFF
TC  = CPD[0..2]                    // linear team colour
k   = CPD[3]                       // per house×colour boost, default 1.8
L   = dot(A, float3(0.2126, 0.7152, 0.0722))   // linear-space luminance
Tint= saturate(TC * L * k)         // clamp: see flags
BaseColor = lerp(A, Tint, TM.r)
// G = chrome: Metallic = lerp(ORM.b, 1, TM.g); Roughness = lerp(ORM.g, ChromeRough(0.18), TM.g)
```

**CPD layout.** Index 0–2 holds TeamColour as **linear** RGB, index 3 holds `k`, and index 4 is reserved for a selection or damage flash. Set the default values on the material's CPD params so editor previews aren't black.

**Who sets CPD.** On spawn, after `TeamId` is known, a `UKodTeamPresentationSubsystem` (KodUnits) looks up the `FKodTeamInfo::TeamColor` (already exists) and the `k` from a `DT_KodTeamTintK` table (house × colour, see §8). It writes CPD to all of the actor's primitives.

**Texture settings.** `T_<Asset>_TM` uses sRGB **off** and Compression = `Masks` (BC1, linear) by default. If soft-edge banding shows on 0.2–0.8 gradients, use `BC7` (sRGB off). Don't use the `Normalmap` setting to get BC5: it applies normal-map handling. B and A stay spare, and unused channels don't increase the cost of BC1.

**Stub first.** Proxy cube and Marine with team 0 and team 1, set through CPD. Check that an ISM of 50 proxy buildings with two teams renders in one draw per material through `PerInstanceCustomData`.

**Pitfalls.**
- ISM and HISM use the `PerInstanceCustomData` node, while components use the `CustomPrimitiveData` node. That needs a `StaticSwitch` `bInstanced` in the master, or two masters.
- Skinned meshes don't batch with each other anyway. CPD on skeletal mesh components is still preferred (one material instance, no MID churn), but verify that CPD is honoured on `USkeletalMeshComponent` in 5.8 *[assumed supported; test on day 1]*. The fallback for units only is a single MID per actor.

---

## 8. Data tables for Strategy numbers

**CALL.**
- **DataAssets stay the identity**: bare id, mesh and anim soft refs, tags, abilities, footprint.
- **CSV-backed `UDataTable`s carry the numbers Strategy tunes.** They are keyed by the same bare id and override DA numbers when the catalog loads.
- This gives one pipeline (CSV in git → DT import → catalog). No spreadsheet plugins and no external tools.

**Add**
- Row structs (KodUnits/Data):
  - `FKodUnitStatsRow` (MaxHealth, Armor, MoveSpeed, CostJadeite, CostLuminene, SupplyCost, BuildSeconds, SightRadius, CollisionRadius)
  - `FKodBuildingStatsRow` (+ FootprintX/Y, SupplyProvided, BuildSeconds)
  - `FKodWeaponStatsRow`
  - `FKodAbilityStatsRow`
- Tables: `DT_KodUnitStats`, `DT_KodBuildingStats`, `DT_KodWeaponStats`, `DT_KodAbilityStats`, `DT_KodTeamTintK` under `/Game/Warden/Data/Tables/`. CSV source of truth lives in `ue/KingdomOfDust/Data/Balance/*.csv`. The CSVs are text and diffable, and Strategy edits them.
- `UKodDataCatalog::ApplyStatOverrides()` runs after DA resolve, **and on the `UKodSlice0Bootstrap` path too**. Seconds are converted with `KodBuildTicks::SecondsToBuildTicks`, so Strategy writes seconds and the sim gets ticks.
- `KodDataValidation` adds checks that every row name matches an existing DA id, that every DA has a row (warn), and that costs are non-negative.

**Hot reload.**
- In the editor, reimporting the CSV fires `UDataTable::OnDataTableChanged`. The catalog re-applies values to new spawns.
- Live entities are only refreshed between PIE sessions, or with the console command `Kod.Data.Reapply` (team-wide, logged, which breaks the idle hash by design).
- Never auto-patch mid-match.

**Ownership.**
- Strategy owns the CSV values.
- Lead Dev owns the row schemas and the import.
- SME gates schema changes and checks that no stat is read from anywhere except the catalog.

**Pitfalls.**
- Row names are case-sensitive `FName`s. `Ranger` ≠ `ranger`.
- **The Marine rename:** keep the id `Ranger` this push. Set `DisplayName = "Marine"`. Do a later id rename pass with `CoreRedirects` plus a CSV row rename in a single commit.
- Don't let CSV override asset refs. Numbers only.

---

## Week-1 build order (Lead Software Dev)

Each step ends with a PIE smoke test and a stable idle hash. SME gate-reviews the steps marked ★.

1. ★ **Unify the ledger (§3).**
   - Sim team state: `Bank`, `SupplyUsed`, `SupplyCap`.
   - `FKodResourceCost Cost` and `SupplyCost/SupplyProvided` on the defs.
   - Retire `UKodResourceWallet` as an authority.
   - *Blocks everything else.*
2. **Commands on the sim tick (§1).**
   - `ExecuteTick`, ordered drain inside `StepSim`, the `Build` → `PlaceStructure`/`Train` split, per-entity `OrderQueue` + shift-queue.
   - Route `IssueMoveToSelection` through commands.
3. ★ **Sim production queue (§5).**
   - 5 slots, spend/refund, supply block, deferred spawn, rally.
   - CC→Dozer, Barracks→Ranger(Marine).
   - The command card's Train button issues `Train`.
4. **Enhanced Input + single selection (§1).**
   - `IMC_KodRTS` and `IA_*`, delete `LocalSelection`, control groups 0–9, legacy poll behind the CVar.
5. ★ **Build grid + placement (§4).**
   - `UKodBuildGridSubsystem`, `AKodPlacementGhost`, the Dozer builds a Barracks and a 2×2 supply pad.
   - Construction ticks, drop-off registration on completion.
6. **Movement v1 (§2).**
   - Spatial hash, separation, arrival spread, clamp to the occupancy grid.
   - Per-node harvester occupancy (§3).
7. **CSV stat overrides (§8).**
   - Row structs, `DT_KodUnitStats` and `DT_KodBuildingStats`, apply in the catalog and the bootstrap.
   - Strategy's placeholder numbers go in.
8. **One ability (§6)** on the sim ability runtime, plus the GAS gate (no Health read from the ASC).
9. **Team tint (§7).** `MF_KodTeamTint` + CPD on proxies and the Marine. Replaces the MID `ApplyTeamColor`.
10. *(Stretch)* Grid A* group pathing (§2 step 2).

**Definition of playable loop.** From the start you can:
- train Dozers
- gather to afford a Barracks
- place it on the grid
- build a supply pad
- queue 5 Marines with a rally
- shift-queue a move/attack
- bind army to group 1
- trigger an ability

All of this runs through commands, with no NavMesh and no CMC.

---

## Art TM flags

1. **Clipping.** `TeamColour × lum × k` with k at 1.8–2.4 exceeds 1.0 when albedo luminance is above about 0.42–0.55 (white #CACACA ≈ 0.59 linear; with k=2.4 it clips at L > 0.7). The shader `saturate()`s, so bright zones flatten to flat colour. **Ask:** team zones stay on mid-value paint, linear luminance ~0.15–0.40, which matches Art's own "mid-value materials" rule. Validate with a debug view.
2. **k is data, not a constant.** Per-house × colour k (2.2–2.4 for orange/salmon/white on CN, orange/sky on Insurgents) goes in `DT_KodTeamTintK` and is fed through CPD[3]. Art supplies the table, not a material edit.
3. **Hex colours are sRGB.** Code converts them with `FLinearColor::FromSRGBColor` before writing CPD. Art should judge colours in-engine, not from the hex in Photoshop.
4. **G = chrome supersedes the 2026-09-30 convention.** That convention reserved G for wear/AO (`TEAM_COLOR_UE_MASK_CONVENTION_2026-09-30.md`), which needs a one-line update. **Ask Art:** what chrome does in the shader. SME proposal: it forces Metallic→1 and Roughness→~0.18 over ORM. Also confirm that ORM packing (R=AO, G=Rough, B=Metal) *[assumed; no Marine B material in repo]* stays separate, and that chrome isn't also baked into ORM.
5. **The existing code contradicts the spec.** `docs/04-slice0-warden.md` and `AKodUnit::ApplyTeamColor` treat team colour as "the strong base on large armour areas" with a per-actor MID. The Art lock says highlights only, 10–20% of the surface for every faction. Code will follow the Art lock.
6. **TM must not be sRGB** and must not live in albedo alpha. Use `Masks` compression, or `BC7` if soft edges band. Never use the Normalmap/BC5 setting.
7. **Nanite/ISM:** fine for buildings with CPD/PerInstanceCustomData. Units are skeletal and non-Nanite. Don't author unit TMs assuming Nanite.
8. **Never-mask list is enforced only by painting.** That covers glow openings, Insurgent eyes, Jadeite and house accents, because the shader can't tell those regions apart. A TM review sits on the 64 px read-check strip.

---
*Verified on `main` @ a7ca04d vs assumed: see the SME report-back. No code was written and no PR was opened.*

---
## TM spec, locked with Art (2026-10-11 8:40 PM PT)
This section supersedes the Art TM flags above where they conflict.
- `T_<Asset>_TM` channels: R = team mask, G = wear/AO (the 2026-09-30 convention stands; G is not chrome), B = metal-trim mask. Settings: sRGB off, `Masks` compression, never stored in albedo alpha.
- Trim (TM.b) lerps on top of ORM: Metallic goes to 1, Roughness to about 0.18, and BaseColor to the per-house trim tint (`DT_KodHouseTrim`: Vanguard brass from style guide v2, Dynasty gold #E3C34E, Scinder none/rust). ORM itself is untouched. Marine B v5's separate CM map gets repacked into TM.b.
- Team tint: `BaseColor = saturate(lerp(Albedo, TeamColour * lum(Albedo) * k, TM.r))`. k is stored per house and per colour in `DT_KodTeamTintK` (default about 1.8). Weak pairs need k of about 2.2–2.4: white on Dynasty, orange or salmon on Dynasty copper, and orange or sky on Scinder.
- Under TM.r the albedo must be a desaturated mid grey or warm neutral (linear luminance about 0.15–0.40). Vanguard v2's solid crimson mask zones are authoring-only and get baked to neutral grey.
- Coverage: team colour covers 10–20% of the surface for every faction, on designated zones (shoulders, upper plates, roof panels, select edges). It is not the main armour colour. `ApplyTeamColor` changes to follow this.
- Art reviews the never-mask zones on the 64 px strip as each asset lands; send them the asset paths.
