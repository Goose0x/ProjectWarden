# Kingdom of Dust — Unreal Engine 5 project

Clean-room **UE 5.8** C++ RTS skeleton. Original Kod\* code only — **no** GPL / ZH GameCode under `Source/`.

Companion docs (repo root):

- [Architecture & M1 acceptance](../../docs/01-zh-to-unreal-architecture.md) (§7 Sandbox Skirmish Slice)
- [Project skeleton](../../docs/02-ue-project-skeleton.md)
- [Scaffold status](../../docs/03-scaffold-status.md)

## Open & build (on your machine)

1. Install **Unreal Engine 5.8** (project EngineAssociation).
2. Right-click `KingdomOfDust.uproject` → **Generate Visual Studio project files** (Windows) or use Unreal’s project browser on Mac/Linux.
3. Open `KingdomOfDust.uproject` in the editor (or build `KingdomOfDustEditor` from IDE).
4. First open will compile C++ modules (`KodCore`, `KodUnits`, `KodEconomy`, `KodGenerals`, `KodAI`, `KodNet`, `KodUI`, `KodEditor`, `KingdomOfDust`).
5. Create **Content** assets listed under each `Content/**/README.md` (no binary `.uasset` files are in git — editor only).

### Default framework classes

Configured in `Config/DefaultEngine.ini` / `AKodGameMode`:

| Slot | Class path |
|------|------------|
| Game Instance | `/Script/KodCore.KodGameInstance` |
| Game Mode | `/Script/KodCore.KodGameMode` |
| Game State / PC / PS / HUD | `AKodGameState`, `AKodPlayerController`, `AKodPlayerState`, `AKodHUD` |

### Plugins enabled in `.uproject`

Enhanced Input, Gameplay Abilities, CommonUI, AI Support, EQS Editor, StateTree, Niagara, Modeling Tools Editor Mode.

## M1 acceptance (Sandbox Skirmish)

From [architecture §7](../../docs/01-zh-to-unreal-architecture.md):

1. Box-select ≥2 units; Ctrl+# assign; # recall  
2. Right-click move; attack target  
3. Spend resource to queue 1 unit from 1 building  
4. Activate 1 general power (GAS Dust Storm) with cooldown on command card  
5. FOW hides enemies until vision touches  
6. **Zero GPL code** under `Source/`; no ZH / Generals GameCode tree in this repo  

All orders must go through `UKodCommandSubsystem` even offline.

## Module map

```
KodCore → KodUnits / KodEconomy / KodAI / KodGenerals / KodNet
                ↘____________ KodUI ____________↙
KingdomOfDust (primary game module) depends on all runtime Kod*
KodEditor — EditorOnly
```

## Naming

Product-facing names: **Kingdom of Dust** / `Kod*`. No EA / C&C / Generals trademarks in APIs or UI strings.

## License stance

Proprietary game code. The GPL Generals / Zero Hour reference is not in this repository and must stay that way.
