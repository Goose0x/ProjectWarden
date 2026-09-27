# Kingdom of Dust — ZH Systems Template → Unreal-First RTS Design

**Status:** Reference analysis + clean-room Unreal design only  
**Date:** 2026-09-25  
**Audience:** Lead Software Dev  

*Kingdom of Dust* is an **Unreal Engine–first, next-gen RTS** (StarCraft 2–like chassis + Generals flavor).  
Command & Conquer Generals / Zero Hour GameCode is a **systems template** used to discover required RTS behaviors and gaps — **not** a line-for-line port target.

**Method:** (1) inventory ZH systems → (2) map each concern to the best modern UE subsystem → (3) invent original KoD types/data. Prefer UE over reinventing WW3D-era engines.

**Related:** [05-bwapi-to-kod.md](./05-bwapi-to-kod.md) — Brood War API as observe/command / AI systems template (ideas only).

---

## 0. License / do-not-copy (read first)

| Rule | Detail |
|------|--------|
| **GPL v3 + EA additional terms** | Unpacked tree `LICENSE.md` is SPDX `GPL-3.0-or-later`. EA released Generals/ZH under GPL v3 with Section 7 terms: **no EA trademarks / publicity rights**; no claiming affiliation with EA. |
| **GPL vs Unreal EULA** | GPL copyleft is **incompatible** with shipping a closed commercial Unreal game that incorporates GPL code. **Do not paste, translate, or “port” ZH/Generals source into Unreal modules.** |
| **No C&C branding** | No Command & Conquer, Generals, Zero Hour, faction marks, or EA marks in product/UI. Original IP; Generals-**flavor** systems only. |
| **Clean reimplementation in UE** | Study architecture and gameplay *concepts*. Author original C++/Blueprint, DataAssets, and algorithms. |
| **Reference only** | Do **not** vendor ZH / Generals GameCode in ProjectWarden. Never copy headers into `Source/` or embed GPL text in Content. |
| **This zip’s identity** | Unpacks as `GeneralsGameCode-main` (TheSuperHackers/GeneralsGameCode community fork of EA GPL code). Official mirror also exists at `electronicarts/CnC_Generals_Zero_Hour`. Same constraints. |
| **Assets not in zip** | Source + tooling only. No Data INIs/maps/art/audio. Do not scrape retail assets into KoD. |

---

## 1. What we unpacked

Study notes only. **ProjectWarden does not contain this tree** — do not add `reference/zh-gamecode/` or `GeneralsGameCode-main/` to the repo.

### Unpack location (local study machine, not this git repo)

```
/workspace/kingdom-of-dust/reference/zh-gamecode/GeneralsGameCode-main/
```

That private unzip used a single archive root folder, `GeneralsGameCode-main/`.

### Top-level tree overview

| Path | Role |
|------|------|
| `Core/` | Shared engine: `GameEngine`, `GameEngineDevice`, `Libraries`, `Tools` |
| `Generals/` | Base game product overlay (`Code/GameEngine`, `Main`, `Tools`, `Libraries`) |
| `GeneralsMD/` | **Zero Hour** (“Mission Disk”) overlay — primary study target |
| `Dependencies/` | Bink, Miles, MaxSDK, DbgHelp, Utility, … — **ZH-only; UE replaces all** |
| `cmake/`, `scripts/`, `resources/` | Build / CI / packaging |
| `GeneralsReplays/` | Replay-related folder (sparse here) |
| `LICENSE.md`, `README.md`, `CMakeLists.txt`, `vcpkg.json` | Legal + build |

Approx. `.cpp` counts: Core ~899, GeneralsMD ~542, Generals ~498 (includes tools).

### Product layout (Generals & GeneralsMD)

```
Code/
  Main/                 # WinMain.cpp — process entry
  GameEngine/           # Logic + Client + Network + Common
  GameEngineDevice/     # W3D / Miles / Win32 device impls
  Libraries/            # WWVegas extras
  Tools/                # WorldBuilder, GUIEdit, MapCacheBuilder, …
```

### Core RTS heart (what we mine for *behaviors*)

| ZH layer | Path | Behavioral lesson for KoD |
|----------|------|---------------------------|
| **Common** | `Include/Common`, `Source/Common` | Subsystems, data load, command stream, CRC/snapshot |
| **GameLogic** | `GameLogic/` | Simulation objects, AI, scripts, production, combat |
| **GameClient** | `GameClient/` | Selection, control bar, drawables, input translators |
| **GameNetwork** | `GameNetwork/` | Frame commands, connections (era-specific; redesign with UE net) |

`GameEngine` owns Logic + Client + Network (`Common/GameEngine.h`). `GameLogic` is the sim singleton; `MessageStream` pipes orders through translators — the multiplayer-ready idea we keep as **design**, implemented with UE networking.

---

## 2. Real RTS systems discovered in the tree

Use this as a **checklist of behaviors** KoD must cover — then implement with UE (Section 3).

### 2.1 Orchestration & command pipeline

| ZH concept | Where | Behavior to cover |
|------------|-------|-------------------|
| Engine loop | `GameEngine` | Match init / tick / quit |
| Sim world | `GameLogic` | Entity registry, game modes, CRC hooks |
| Message stream | `MessageStream`, `*Xlat`, `HotKey` | Input → ordered commands (not direct sim mutation) |
| Net frame data | `FrameData`, `NetCommandMsg`, `ConnectionManager` | Synchronized orders (lockstep-era) |

### 2.2 Data-driven entities

| ZH concept | Where | Behavior to cover |
|------------|-------|-------------------|
| ThingTemplate / ThingFactory | `Common/ThingTemplate.h` | Archetypes: stats, weapons, prereqs, modules |
| Object + Drawable | `Object.h`, `Drawable.h` | Sim instance vs visual twin |
| ModuleFactory + ~222 modules | `Module/`, `ModuleFactory.h` | Composition: Update, SpecialPower, Upgrade, Contain, … |
| INI parsers | `Source/Common/INI/INI*.cpp` | Object, Weapon, SpecialPower, CommandSet, Upgrade, … |

### 2.3 Players, economy, tech, generals flavor

| Behavior | ZH types |
|----------|----------|
| Players / teams | `Player`, `PlayerList`, `PlayerTemplate`, `Team` |
| Resources / gather | `Money`, `ResourceGatheringManager` |
| Power grid | `Energy` |
| Sciences / general powers | `Science`, `SpecialPower`, `RankInfo`, `Powers` |
| Build queues / prereqs | `ProductionUpdate`, `ProductionPrerequisite` |
| Placement | `BuildAssistant`, place translators |
| Upgrades | `Upgrade`, Upgrade modules, `CommandSetUpgrade` |

### 2.4 Combat, movement, spatial, vision

Weapons/armor sets, locomotor, pathfinder, `PartitionManager`, ghost objects / FOW, radar/minimap.

### 2.5 AI & mission scripting

Unit AI state machines / AIUpdate modules, `AIPlayer` / `AISkirmishPlayer`, `ScriptEngine` + actions/conditions, squads.

### 2.6 Client UX

`ControlBar`, CommandSet/CommandButton, selection + control groups (`DrawGroupInfo`), hotkeys / meta events, in-game UI, EVA-style announcer.

### 2.7 Not in this zip

Full Data INI packs, maps, art, audio — author original KoD content in UE.

---

## 3. Unreal-first system map (ZH concern → UE subsystem → Kod module)

**Principle:** ZH tells us *what* an RTS needs. Unreal tells us *how* for a next-gen title. Custom Kod code only where UE has no good fit (RTS command card, FOW grid, build queues, control groups).

| ZH concern | Prefer Unreal system | Kod module / type | Notes |
|------------|----------------------|-------------------|-------|
| Process / game loop | `UGameInstance`, GameMode flow | **KodCore** `UKodGameInstance`, `AKodGameMode` | No WinMain port |
| Match / lobby state | `AGameState`, `APlayerState`, Sessions | **KodNet** / Core | |
| Sim vs presentation | Actor replication + local views; optional fixed-step subsystem | **KodCore** `UKodSimSubsystem` | Keep gameplay decisions command-driven |
| Input (mouse/keys) | **Enhanced Input** (`UInputMappingContext`, `UInputAction`) | **KodUI** `UKodHotkeyRouter` | Replace HotKey/MetaEvent tables |
| Selection / groups | Custom + Enhanced Input | **KodUI** `UKodSelectionManager`, `UKodControlGroupStore` | SC2-like; ZH only inspired |
| Orders pipeline | Custom command buffer → server or local sim | **KodNet** `UKodCommandSubsystem`, `FKodCommand` | Spiritual heir of MessageStream |
| Unit/building defs (INI Object) | **Primary DataAssets** + **DataTables** + **Gameplay Tags** | **KodUnits** `UKodUnitDefinition`, `UKodBuildingDefinition` | Tags for KindOf / filters |
| Composition (Modules) | **Actor Components** + **GAS** (`UAbilitySystemComponent`) | **KodUnits** | Do not clone ModuleFactory |
| Weapons / damage | GAS **Gameplay Effects** + cue notifies | **KodUnits** | Instant/projectile as abilities or GE |
| General powers / sciences | **Gameplay Ability System** + Tags + GE cooldowns | **KodGenerals** `UKodGeneralPowerSet` | Generals flavor on GAS |
| Buffs / upgrades | Gameplay Effects + Tag-gated unlocks | **KodEconomy** / Units | |
| Build queue / prereqs | Custom components + DataAssets | **KodEconomy** `UKodBuildQueueComponent`, `UKodTechTree` | UE has no RTS builder — invent cleanly |
| Resources / power grid | PlayerState attributes (GAS AttributeSet) or custom | **KodEconomy** `UKodResourceWallet`, `UKodPowerGrid` | |
| Movement / path | **Navigation System** (Detour), RVO; later flow fields if needed | **KodUnits** | Replace ZH Pathfinder |
| Crowd / many units | Evaluate **Mass Entity / MassAI** for background armies; Heroes stay Actors | **KodUnits** / optional Mass plugin | Hybrid: Mass for scale, Actors for interaction |
| Unit AI | **AI Controllers** + **Behavior Trees** + **StateTree** + **EQS** | **KodAI** | Replace AIUpdate spaghetti |
| Commander / skirmish AI | BT/StateTree on commander pawn/controller | **KodAI** `UKodCommanderAI` | |
| Mission scripts | Level BPs, **Smart Objects**, DataAssets, optional custom mission graph | **KodAI** / Content | No ZH ScriptEngine VM |
| FOW / partition | Custom grid subsystem (+ optional GPU) | **KodCore** `UKodFogOfWarSubsystem` | UE has no stock RTS FOW |
| VFX | **Niagara** | Content + Units | Replace particle INI / W3D FX |
| Physics / destruction | **Chaos** (selective — RTS often kinematic) | Units/Buildings | Use for debris/heroics, not every unit |
| Terrain / large maps | **Landscape** + **World Partition** + HLOD | Maps in Content | Replace TerrainLogic + WorldBuilder |
| Audio | **MetaSounds** + MetaSound Sources | Content | Replace Miles |
| Cinematics / video | Sequencer / Media Framework | Content | Replace Bink |
| UI shell + HUD | **UMG** + **CommonUI** | **KodUI** | Replace ControlBar gadgets |
| Command card | UMG widgets bound to DataAsset command sets | **KodUI** `UKodCommandCardWidget` | |
| Minimap | UMG + scene capture / custom | **KodUI** | |
| Networking | **GameMode / GameState / PlayerState**, Actor replication, **dedicated server**; consider **Iris** when scaling relevancy | **KodNet** | Do not port GameSpy/WOL/UDP lockstep blindly |
| Save / replay stubs | **SaveGame**; command log for replay later | **KodCore** | Replace Xfer/Recorder gradually |
| Editor tools | Unreal Editor, Editor Utility Widgets, PCG | Optional **KodEditor** | Discard WorldBuilder/GUIEdit |

### Module dependency sketch

```
KodCore  (GameInstance, SimSubsystem, FOW, Tags shared)
   ↑
KodUnits   KodEconomy   KodAI   KodGenerals   KodNet
   ↑           ↑          ↑          ↑           ↑
   └───────────┴──────────┴──────────┴───────────┘
                        KodUI
KingdomOfDust (game module) depends on all
```

---

## 4. ZH gaps → Unreal fills

Generals/ZH was excellent for its era but constrains a next-gen SC2-like + Generals-flavor RTS. UE closes these gaps — **lean into them**.

| Gap in ZH / Generals | What UE gives KoD |
|----------------------|-------------------|
| Homegrown W3D renderer, limited lighting/PBR | UE5 renderer, Lumen/Nanite (use Nanite judiciously for RTS), modern materials |
| Miles audio + ad-hoc events | **MetaSounds** — procedural, parameter-driven combat/UI mix |
| INI + custom parsers, weak tooling | **DataAssets / DataTables**, editor validation, cook pipelines |
| Monolithic Module C++ zoo (~222 headers) | Components + **GAS** abilities/effects — designer-iterable |
| Custom gadget UI toolkit | **UMG / CommonUI**, gamepad/PC input via CommonUI activatable widgets |
| Win32 + HotKey tables | **Enhanced Input** — contexts, remapping, local multiplayer-ready |
| Hand-rolled pathfind / partition | **NavMesh**, EQS queries, optional Mass for crowds |
| Script VM for missions | Blueprints, StateTree, Sequencer — faster content iteration |
| GameSpy / WOL / brittle NAT era | EOS/Sessions, UE replication, dedicated servers, future Iris relevancy |
| No first-class ability framework | **GAS** for generals powers, unit abilities, upgrades-as-GE |
| Weak tagging / query language | **Gameplay Tags** for KindOf, filters, UI, AI EQS |
| Single large maps / custom WB formats | **World Partition**, streaming, Landscape, HLOD |
| Limited VFX authoring | **Niagara** systems shared across units/powers |
| Client/logic split but ancient devices | Clean **server-auth or command-stream** design without WW device layer |
| No Mass-scale entity story | **Mass Entity / MassAI** option when unit counts demand it |
| Localization / UI scale 2003-era | UE localization, modern HUD scaling, accessibility hooks |
| Determinism via CRC lockstep only | Choose **server-authoritative** (simpler, next-gen default) or later lockstep; don’t inherit ZH net as gospel |

**Design stance:** Build an SC2-like order/selection/hotkey core in UE, then layer Generals-flavor (commander powers, base power, build queues) as GAS + Economy modules — not a ZH executable in Unreal clothing.

---

## 5. Generals-flavor to preserve as DESIGN vs ZH-only

### Preserve as design (reimplement original in UE)

- Commander / general **powers** (GAS abilities, cooldowns, rank/science gates)  
- **Build queues** + prerequisite tech tree  
- **Base building** with power-grid constraints and placement rules  
- **Fog of war** + last-seen ghosts (presentation)  
- Multi-select, **control groups**, smart hotkeys, command card  
- Mixed army + upgrades  
- Original **resource economy** (fantasy framing — not C&C supply/oil clones)  
- Skirmish **commander AI**  
- Command-stream discipline for MP-ready SP  

### Stay ZH-only

- WW3D / Miles / Bink / Win32 device stack  
- GameSpy, WOL, mangler/matchbot  
- Exact Module graph and INI schema  
- EA trademarks, factions, unit names, VO, art  
- WorldBuilder / GUIEdit formats  
- Any GPL source text or retail-compatible CRC logic  

---

## 6. StarCraft 2–like targets (Unreal-native)

| Goal | UE-native approach |
|------|-------------------|
| APM UX | Enhanced Input + control groups + queued `FKodCommand`s |
| Command card | UMG/CommonUI bound to DataAsset sets + Gameplay Tags |
| Sim/presentation | Server or sim subsystem applies commands; views interpolate |
| MP from day one | All orders via `UKodCommandSubsystem` even offline |
| Remappable hotkeys | Enhanced Input mapping contexts per faction/layout |
| Scale | Actors first; profile → Mass for chaff units if needed |
| Replay later | Command log + SaveGame/snapshots — not ZH Recorder copy |

---

## 7. First milestone — Unreal-native smallest playable slice

### Goal

**“Sandbox Skirmish Slice”** in a blank UE5 project: one Landscape map, select/move/attack, one building with a one-slot build queue, one resource, one GAS general power, FOW stub. **Offline**, but every order goes through `UKodCommandSubsystem`.

This is **not** “port `GameLogic.cpp`.” It is “stand up UE systems that cover the behaviors ZH proved an RTS needs.”

### Stubs to create (original Kod types)

| Module | Create | Backed by UE |
|--------|--------|--------------|
| **KodCore** | `UKodGameInstance`, `AKodGameMode`, `AKodGameState`, `UKodSimSubsystem`, `UKodFogOfWarSubsystem`, shared GameplayTags ini | GameInstance / GameMode |
| **KodUnits** | `UKodUnitDefinition` (PrimaryDataAsset), `AKodUnit`, `UKodHealthAttributeSet` (or health component), move/attack via commands | NavMesh, GAS optional early |
| **KodEconomy** | `UKodResourceWallet` on `AKodPlayerState`, `AKodBuilding`, `UKodBuildQueueComponent` | PlayerState |
| **KodGenerals** | One `UGameplayAbility` power + `UKodGeneralPowerComponent` presenting it | **GAS** |
| **KodUI** | Enhanced Input contexts, `UKodSelectionManager`, `UKodControlGroupStore`, `UKodCommandCardWidget` (UMG) | Enhanced Input, UMG |
| **KodNet** | `UKodCommandSubsystem` + `FKodCommand` (Move, Attack, Build, CastPower) — local exec; replication TODO | Ready for GameState later |
| **KodAI** | `AKodUnitAIController` stub issuing Move commands | AI Module |

### Content (not code ports)

- 1x Landscape map with NavMesh bounds  
- 2–3 placeholder meshes (unit, building, resource node)  
- DataAssets (Slice 0): bare ids `Ranger` / `Barracks` / … under `/Game/Warden/Data/`; powers later (`DustStorm`)  
- Niagara stub for the power  
- MetaSound stub for confirm/attack click  

### Exit criteria

1. Box-select ≥2 units; Ctrl+# assign; # recall  
2. Right-click move; attack target  
3. Spend resource to queue 1 unit from 1 building  
4. Activate 1 general power (GAS) with cooldown on command card  
5. FOW hides enemies until vision touches  
6. **Zero GPL code** under `Source/`; do not vendor the ZH reference tree  

### Non-goals (M1)

Networking host/client, full tech tree, air/transport, Mass, World Partition streaming polish, campaign.

---

## 8. Working practices

1. Sections 1–2 are a **behavior checklist**, not a paste source. The GPL tree stays off this repo.  
2. When “how did ZH do X?” → study privately → write a **Kod design note** → implement with UE APIs. Do not copy headers into `Source/`.  
3. Default stack: **Enhanced Input + GAS + Tags + DataAssets + UMG/CommonUI + NavMesh**. Custom only for RTS specifics (FOW, build queue, control groups, command card).  
4. Public APIs use Kod / Kingdom of Dust names only.  
5. Legal review before any content that could be argued derivative.

---

## 9. Appendix — high-value ZH files (read-only)

| Topic | Path |
|-------|------|
| Engine loop | `GeneralsMD/Code/GameEngine/Include/Common/GameEngine.h` |
| Sim world | `GeneralsMD/.../GameLogic/GameLogic.h` |
| Templates | `GeneralsMD/.../Common/ThingTemplate.h` |
| Modules | `GeneralsMD/.../Common/ModuleFactory.h` |
| Player/economy | `GeneralsMD/.../Source/Common/RTS/*.cpp` |
| Messages | `Core/GameEngine/Include/Common/MessageStream.h` |
| Control bar | `Core/GameEngine/Include/GameClient/ControlBar.h` |
| INI surface | `Core/GameEngine/Source/Common/INI/` |
| Special powers | `GeneralsMD/.../Object/SpecialPower/` |
| Production | `GeneralsMD/.../Update/ProductionUpdate.cpp` |

