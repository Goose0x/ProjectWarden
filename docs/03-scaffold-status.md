# Kingdom of Dust — UE scaffold status

**Date:** 2026-09-25 (PT)  
**Location:** `ue/KingdomOfDust/` (this repo)  
**Status:** C++ skeleton + Content placeholders created. **Not yet compiled** (no UE toolchain on scaffold host).

## Created

### Project root

| Artifact | Path |
|----------|------|
| `.uproject` | `ue/KingdomOfDust/KingdomOfDust.uproject` |
| README | `ue/KingdomOfDust/README.md` |
| Config | `Config/DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`, `DefaultGameplayTags.ini` |
| Targets | `Source/KingdomOfDust.Target.cs`, `KingdomOfDustEditor.Target.cs` |

### Modules (9)

| Module | Type | API macro | Notes |
|--------|------|-----------|-------|
| KingdomOfDust | Runtime (primary) | — | `IMPLEMENT_PRIMARY_GAME_MODULE` |
| KodCore | Runtime | `KODCORE_API` | GameInstance/Mode/State/PC/PS/HUD, Sim, FOW, Tags |
| KodUnits | Runtime | `KODUNITS_API` | Definitions, Unit/Building, move/attack/ASC, combat attrs |
| KodEconomy | Runtime | `KODECONOMY_API` | Wallet, power grid, **one-slot build queue**, tech/upgrade/gather |
| KodGenerals | Runtime | `KODGENERALS_API` | PowerSet, PowerComponent, **GA_KodPower_DustStorm** |
| KodAI | Runtime | `KODAI_API` | Unit + Commander AI controllers (command-subsystem orders) |
| KodNet | Runtime | `KODNET_API` | **FKodCommand / EKodCommandType**, CommandSubsystem enqueue/process |
| KodUI | Runtime | `KODUI_API` | SelectionManager, ControlGroupStore, HotkeyRouter, HUD widgets |
| KodEditor | **EditorOnly** | `KODEDITOR_API` | Data validation stub |

### M1 stubs with real API surface

- `UKodCommandSubsystem` + `FKodCommand` / `EKodCommandType` (Move, Attack, Stop, Build, CastPower, …)
- `UKodFogOfWarSubsystem` (grid reveal / visibility query)
- `AKodUnit` / `AKodBuilding` with definition soft refs + entity registration
- `UKodBuildQueueComponent` one-slot enqueue / tick / complete
- `UGA_KodPower_DustStorm` GAS ability stub (`Kod.Power.DustStorm`)
- `UKodSelectionManager` + `UKodControlGroupStore` (assign / recall)
- `AKodPlayerController` Enhanced Input IMC push + comments for command enqueue

### Content

Folders mirror `02-ue-project-skeleton.md`. Each leaf has a **README.md** describing assets to create in-editor. **No** binary `.uasset` / `.umap` files.

USA Recon hover v2.1 is **HOLD** — Director rejected it after QA PASS. See [06-usa-recon-anim-lock.md](06-usa-recon-anim-lock.md). That Drive folder is not ship loco. Slice 0 still uses the Ranger PROXY.

## Still editor-only / manual

- Landscape map `L_SandboxSkirmish` + NavMesh  
- IMC / IA Enhanced Input assets  
- Slice 0 Warden bare ids under `/Game/Warden/Data/` (`RangerRifle`, `Ranger`, `Dozer`, `CommandCenter`, `Barracks`, `USA`) — see `04-slice0-warden.md`
- `DA_PowerSet_Starter` (later; not Slice 0 gate)  
- `BP_KodUnit`, `BP_KodBuilding`  
- WBP shells for HUD, Main Menu, and Lobby Versus — C++ parents and BindWidget names are in [docs/ui/UE_CONSTRUCTION.md](ui/UE_CONSTRUCTION.md). Create them in the Editor; do not commit `.uasset` files.
- Niagara `NS_DustStorm`, MetaSounds  
- Generate IDE project files & first compile on a machine with UE 5.8  
- Wire PC input bindings → selection → `UKodCommandSubsystem::Enqueue` end-to-end  
- Attach `UKodBuildQueueComponent` on building BP; wallet on PlayerState  
- FOW presentation (mesh darkening / last-seen ghosts)

## Hard rules verified at scaffold time

- No ZH / Generals GameCode under `Source/`  
- No EA / C&C trademark product strings (Kingdom of Dust / Kod* only)  
- GPL reference tree is not part of this repo  

## Suggested first steps (dev machine)

1. Generate project files; open in UE 5.8; fix any module/plugin version mismatches.  
2. Create `L_SandboxSkirmish` + NavMesh; PIEable empty level.  
3. Author IMC/IA; bind on `AKodPlayerController` (or BP).  
4. Create Warden DAs (bare ids) or use `UKodSlice0Bootstrap`; place PROXY units on `L_Slice0`.  
5. Drive select → move → build queue → Dust Storm through the command subsystem.  
6. Tick M1 exit criteria in `01-zh-to-unreal-architecture.md` §7.
