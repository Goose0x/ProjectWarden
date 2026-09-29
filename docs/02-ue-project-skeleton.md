# Kingdom of Dust — Unreal Project Skeleton

**Status:** Proposed layout for Lead Software Dev  
**Date:** 2026-09-25  
**Companion:** `01-zh-to-unreal-architecture.md`

Unreal-first next-gen RTS. ZH GameCode informs *which* systems exist; this skeleton is native UE5 (C++ modules + Content). **No GPL source under `Source/`.**

---

## Project

| Field | Value |
|-------|-------|
| **Project name** | `KingdomOfDust` |
| **`.uproject`** | `KingdomOfDust.uproject` |
| **Engine** | UE 5.8 (team target / EngineAssociation) |
| **Primary type** | C++ code project (Blueprint OK for content/UI polish) |
| **Default map** | `/Game/Maps/Sandbox/L_SandboxSkirmish` |
| **License stance** | Proprietary / closed. Do not vendor ZH / Generals GameCode in this repo or inside packaged Content |

### Enabled plugins (recommended M1+)

| Plugin | Why |
|--------|-----|
| Enhanced Input | Hotkeys, camera, orders |
| GameplayAbilities (+ Tags, Tasks) | Generals powers, weapons-as-abilities, GE buffs |
| CommonUI | HUD / command card / menus |
| AI Module / Environment Querying | Unit + commander AI |
| StateTree (optional M1, strong M2) | Cleaner AI/mission flow |
| Niagara | Power/combat VFX |
| Modeling Tools / Landscape | Map authoring |
| (Later) Mass Entity / MassAI | Crowd-scale units when Actors bottleneck |
| (Later) Online Subsystem / EOS | Multiplayer sessions |
| (Later) Iris | Relevancy at scale — evaluate when net spike comes |

---

## C++ modules (`Source/`)

Target dependency direction: **Core → feature modules → UI/Game**.

```
Source/
  KingdomOfDust.Target.cs
  KingdomOfDustEditor.Target.cs
  KingdomOfDust/
    KingdomOfDust.Build.cs          # Game module — depends on all Kod*
    KingdomOfDust.cpp
    KingdomOfDust.h
  KodCore/
    KodCore.Build.cs
    Public/
      KodCore.h
      Game/
        KodGameInstance.h
        KodGameMode.h
        KodGameState.h
        KodPlayerController.h
        KodPlayerState.h
        KodHUD.h
      Sim/
        KodSimSubsystem.h
        KodEntityId.h
        KodTeamInfo.h
      Vision/
        KodFogOfWarSubsystem.h
      Tags/
        KodGameplayTags.h           # native tag registration helpers
    Private/
      ...
  KodUnits/
    KodUnits.Build.cs               # → KodCore, GameplayAbilities, NavigationSystem
    Public/
      Data/
        KodUnitDefinition.h         # UPrimaryDataAsset
        KodBuildingDefinition.h
        KodWeaponDefinition.h
      Actors/
        KodUnit.h
        KodBuilding.h
      Components/
        KodMoveComponent.h
        KodAttackComponent.h
        KodAbilitySystemComponent.h # or thin wrapper
      Attributes/
        KodCombatAttributeSet.h
    Private/
      ...
  KodEconomy/
    KodEconomy.Build.cs             # → KodCore, KodUnits
    Public/
      KodResourceWallet.h
      KodPowerGrid.h
      KodBuildQueueComponent.h
      KodTechTree.h
      KodUpgradeDefinition.h
      KodGatherComponent.h
    Private/
      ...
  KodGenerals/
    KodGenerals.Build.cs            # → KodCore, GameplayAbilities, KodUnits
    Public/
      KodGeneralPowerSet.h          # DataAsset listing GA classes / tags
      KodGeneralPowerComponent.h
      Abilities/
        GA_KodPower_DustStorm.h     # M1 example
    Private/
      ...
  KodAI/
    KodAI.Build.cs                  # → KodCore, KodUnits, AIModule, (StateTree)
    Public/
      KodUnitAIController.h
      KodCommanderAIController.h
      KodEQS/                       # shared EQS helpers later
    Private/
      ...
  KodNet/
    KodNet.Build.cs                 # → KodCore
    Public/
      KodCommandSubsystem.h
      KodCommandTypes.h             # FKodCommand, EKodCommandType
      KodCommandSerializers.h       # future replication
    Private/
      ...
  KodUI/
    KodUI.Build.cs                  # → KodCore, KodUnits, KodEconomy, KodGenerals, KodNet, UMG, CommonUI, EnhancedInput
    Public/
      KodSelectionManager.h
      KodControlGroupStore.h
      KodHotkeyRouter.h
      KodCommandCardWidget.h
      KodMinimapWidget.h
      KodResourceBarWidget.h
    Private/
      ...
  KodEditor/                        # optional; Editor-only module
    KodEditor.Build.cs
    Public/
      KodDataValidation.h
    Private/
      ...
```

### `.Build.cs` notes

- `KodCore` public deps: `Core`, `CoreUObject`, `Engine`, `GameplayTags`.  
- `KodUnits` / `KodGenerals`: `GameplayAbilities`, `GameplayTags`, `GameplayTasks`, `NavigationSystem`.  
- `KodUI`: `UMG`, `CommonUI`, `EnhancedInput`, `Slate`.  
- `KodAI`: `AIModule`, `GameplayTasks`; add `StateTreeModule` when adopted.  
- `KodEditor`: `UnrealEd`; guard with `Target.bBuildEditor`.  
- Never add a dependency path to `reference/zh-gamecode`.

### Target modules list (`.uproject` Modules array)

1. `KingdomOfDust` (Runtime, Default)  
2. `KodCore`  
3. `KodUnits`  
4. `KodEconomy`  
5. `KodGenerals`  
6. `KodAI`  
7. `KodNet`  
8. `KodUI`  
9. `KodEditor` (Editor only)

---

## Content layout (`Content/`)

```
Content/
  Maps/
    Sandbox/
      L_SandboxSkirmish.umap
    Multiplayer/                    # later
    Campaign/                       # later
  Core/
    Input/
      IMC_KodRTS.uasset             # Enhanced Input mapping context
      IA_Select.uasset
      IA_CommandMove.uasset
      IA_ControlGroup_1..0.uasset
      IA_Camera*.uasset
    Tags/
      # Gameplay Tag tree authored in Project Settings / ini
    GameModes/
      GM_SandboxSkirmish.uasset
      GS_Sandbox.uasset
  Units/
    Definitions/
      # Slice 0 gate DAs are bare ids under /Game/Warden/Data/ (Ranger, Dozer) — not DA_Unit_*
    Blueprints/
      BP_KodUnit.uasset
    Meshes/
    Anims/
    Niagara/
  Buildings/
    Definitions/
      # Slice 0: Barracks / CommandCenter live under /Game/Warden/Data/
    Blueprints/
      BP_KodBuilding.uasset
    Meshes/
  Economy/
    DA_Resource_Cash.uasset           # Playtest #1 primary spend wallet
    DA_Resource_Oil.uasset            # second wallet stub; no gather loop
    DataTables/
      DT_TechPrereqs.uasset         # optional; or keep on DataAssets
  Generals/
    Powers/
      DA_PowerSet_Starter.uasset
      GA_DustStorm.uasset           # Blueprintability OK
    Niagara/
      NS_DustStorm.uasset
  UI/
    HUD/
      WBP_KodHUD.uasset
      WBP_CommandCard.uasset
      WBP_ResourceBar.uasset
      WBP_Minimap.uasset
    CommonUI/
      WBP_Activatable*.uasset
    Styles/
  AI/
    BT_UnitBasic.uasset
    BT_CommanderSkirmish.uasset     # M2+
    EQS/
  Audio/
    MetaSounds/
      MS_UI_Click.uasset
      MS_Unit_Ack.uasset
  FX/
    Niagara/
  Developers/                       # local scratch; not shipped
  Collections/
```

### Data replaces ZH INI

| ZH INI-ish surface | KoD Content |
|--------------------|-------------|
| Object.ini / ThingTemplate | `UKodUnitDefinition` / `UKodBuildingDefinition` PrimaryDataAssets |
| Weapon.ini | `UKodWeaponDefinition` or ability costs on GA |
| SpecialPower.ini | GAS abilities + `UKodGeneralPowerSet` |
| CommandSet / CommandButton | DataAsset command sets + UI widgets |
| Upgrade.ini | `UKodUpgradeDefinition` + GE |
| Science.ini | Tag-gated tech / power unlocks on tech tree asset |
| Multiplayer.ini / GameData | GameState defaults + Developer Settings (`UKodRuntimeSettings`) |

---

## Default gameplay framework classes

Set in Project Settings / GameMode:

| Slot | Class |
|------|-------|
| Game Instance | `UKodGameInstance` |
| Game Mode | `AKodGameMode` |
| Game State | `AKodGameState` |
| Player State | `AKodPlayerState` |
| Player Controller | `AKodPlayerController` |
| HUD | `AKodHUD` (hosts UMG) |

`AKodPlayerController` owns: Enhanced Input bind → `UKodHotkeyRouter` / selection → enqueue `FKodCommand` on `UKodCommandSubsystem`.

---

## M1 folder priority (create first)

1. Modules: `KodCore`, `KodNet`, `KodUnits`, `KodEconomy`, `KodGenerals`, `KodUI` (empty `KodAI` OK).  
2. Content: `L_Slice0`, IMC + few IAs, Warden bare-id DAs, `WBP_KodHUD` + command card.  
3. One GAS ability power end-to-end.  
4. FOW subsystem stub (grid + simple reveal).  

Defer: `KodEditor`, Mass, World Partition multi-region, EOS, Iris, campaign maps.

---

## Repo layout (ProjectWarden)

```
docs/                               # architecture + Slice 0
ue/KingdomOfDust/                   # UE 5.8 .uproject, Source, Config, Content placeholders
```

The GPL Generals / Zero Hour tree is **not** in this repository. Do not add it, and do not copy ZH `.cpp/.h` into `Source/`.

---

## Naming conventions

| Kind | Pattern | Example |
|------|---------|---------|
| Modules | `Kod*` | `KodEconomy` |
| Types | `AKod*`, `UKod*`, `FKod*`, `EKod*` | `UKodBuildQueueComponent` |
| DataAssets (Slice 0) | bare id | `Ranger`, `Barracks`, `USA` under `/Game/Warden/Data/` |
| Widgets | `WBP_` | `WBP_CommandCard` |
| Input | `IMC_`, `IA_` | `IA_CommandMove` |
| Gameplay Tags | `Kod.Unit.*`, `Kod.Power.*`, `Kod.Resource.*`, `Kod.UI.*` | `Kod.Resource.Cash`, `Kod.Power.DustStorm` |
| Abilities | `GA_Kod*` | `GA_KodPower_DustStorm` |

Never ship public APIs named `GameLogic`, `ThingTemplate`, `SpecialPowerModule`, etc.

---

## Next concrete step after skeleton

Generate the UE C++ project (or add modules to a blank UE5 C++ RTS template), then implement M1 stubs listed in §7 of `01-zh-to-unreal-architecture.md` — Unreal-native, clean-room only.
