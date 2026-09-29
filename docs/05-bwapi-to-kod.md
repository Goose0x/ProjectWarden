# Kingdom of Dust — BWAPI Systems Template → KoD Design Notes

**Status:** Reference analysis + clean-room Unreal design only  
**Date:** 2026-09-26  
**Audience:** Lead Software Dev (+ Strategy/AI later)  
**Source:** [bwapi/bwapi](https://github.com/bwapi/bwapi) (Brood War API) — public headers studied remotely; **no clone into this tree**

*Kingdom of Dust* is an **Unreal Engine–first RTS** (StarCraft 2–like chassis + Generals flavor).  
BWAPI is a second **systems template** beside Generals/ZH GameCode: it shows how competitive RTS bots **observe** a frame and **issue** orders under fog. It is **not** a port target and **not** a runtime dependency.

**Sibling doc:** [01-zh-to-unreal-architecture.md](./01-zh-to-unreal-architecture.md) (ZH → UE stack).  
**Slice 0 law:** [04-slice0-warden.md](./04-slice0-warden.md) / [slice0/SLICE0_SHOP_FLOOR.md](./slice0/SLICE0_SHOP_FLOOR.md).

---

## 0. License / do-not-copy (read first)

| Rule | Detail |
|------|--------|
| **LGPL-3.0** | BWAPI is licensed LGPL-3.0. Linking or shipping BWAPI (or derived headers) inside a closed Unreal game is a legal landmine — **do not**. |
| **No Blizzard IP** | StarCraft / Brood War are Blizzard trademarks. No SC unit names, racial marks, or “BWAPI bot” framing in product/UI. |
| **No process injection** | Chaoslauncher, DLL injectors, classic 1.16.1 hooks — irrelevant to UE. Never study those for KoD runtime. |
| **Clean reimplementation** | Steal *shapes*: observe/command split, typed orders, fog-default vision, type tables as data. Author original `FKodCommand`, sim ticks, DataAssets. |
| **No Source/ paste** | Do not copy BWAPI headers into `ue/KingdomOfDust/Source/`. Keep notes here; optional bookmarks only under `docs/`. |
| **Not match truth** | Brood War pathing, latency frames, and bullet tables are not KoD sim law. Slice 0 remains: **16 Hz** `UKodSimSubsystem`, seek/stop, hitscan, idle hash — no NavMesh/CMC-as-truth. |

---

## 1. What BWAPI is (one paragraph)

BWAPI is a C++ framework so AI modules can play *StarCraft: Brood War*. Bots implement `AIModule` (esp. `onFrame`), read a **visible** game snapshot via `Game` / shared `GameData`, and enqueue **unit commands**. By default fog denies unseen units and user input is blocked (tournament modules can enforce harder rules). Docs: http://bwapi.github.io/

Useful mental model for KoD: **bot = AI / input layer**, **GameData = replicated-or-local observation**, **UnitCommand queue = `UKodCommandSubsystem`**, **engine frame = fixed sim tick**.

---

## 2. Surfaces we studied (remote)

| Surface | Path (upstream) | Lesson |
|---------|-----------------|--------|
| Command enum | `bwapi/include/BWAPI/UnitCommandType.h` | Flat, typed order vocabulary (Move, Attack_Unit, Train, Gather, …) |
| AI lifecycle | `bwapi/include/BWAPI/AIModule.h` | `onStart` / `onFrame` / unit show-hide / complete events |
| Shared snapshot | `bwapi/include/BWAPI/Client/GameData.h` | Contiguous observe buffer + outbound command arrays |
| Type catalogs | UnitType / WeaponType / TechType (same include tree) | Static tables ≈ our Warden Primary DataAssets |

---

## 3. Observe vs command (primary takeaway)

BWAPI’s client `GameData` is roughly:

1. **Inbound / observe:** players, units, bullets, map tiles (walkable/buildable/visible/explored), selection, frame counters, events.  
2. **Outbound / command:** `unitCommands[]`, plus debug shapes/strings.

KoD mapping (already sketched in scaffold):

| BWAPI idea | KoD target |
|------------|------------|
| Read-only frame snapshot | Sim / replication view the AI and UI may read — **not** actor-local “I MoveTo so I’m truth” |
| `unitCommands` queue | `UKodCommandSubsystem` + `FKodCommand` |
| `onFrame` | Consume pending commands on **16 Hz** sim tick (`UKodSimSubsystem`), with catch-up cap |
| Fog defaults | Future vision/FOW: AI and UI only see allowed entities (Slice 0 may stay full-info) |
| TournamentModule flags | Later ranked / skirmish rule modules (deny “full map cheat”, cap APM, etc.) |

**Design rule:** PlayerController / Enhanced Input / UI **enqueue** commands; sim **applies** them. Presentation (CMC, Niagara, anim) follows sim — same as Slice 0 shop-floor law.

---

## 4. Command vocabulary → `EKodCommandType`

BWAPI exposes a large `UnitCommandTypes::Enum` (Attack_Move, Attack_Unit, Build, Train, Morph, Research, Move, Patrol, Hold_Position, Stop, Gather, Load/Unload, Use_Tech_*, cancels, race-specific toggles, …).

KoD Slice 0 / early scaffold (`KodCommandTypes.h`) is intentionally thin:

| `EKodCommandType` | Closest BWAPI cousins | Notes |
|-------------------|----------------------|-------|
| `Move` | Move, Right_Click_Position, Attack_Move (move half) | Seek/stop with Accept×0.5 |
| `Attack` | Attack_Unit, Attack_Move (engage), Right_Click_Unit | Hitscan for now; Armor deferred |
| `Stop` | Stop, Hold_Position (partial) | Clear seek target |
| `Build` | Build, Train, Morph (production family) | PayloadName → unit/building DA id |
| `CastPower` | Use_Tech / Use_Tech_Position / Use_Tech_Unit | Generals-flavor powers later |
| `Gather` | Gather, Return_Cargo | Economy pass |
| `SetRally` | Set_Rally_Position / Set_Rally_Unit | Barracks / CC |

**Do not** mirror every BWAPI enum value into Unreal. Grow `EKodCommandType` when a gameplay slice needs it (Patrol, Follow, Load, Research as first-class, cancel slots, …). Prefer **data-driven** tech/unit abilities over exploding the enum for every race quirk.

`FKodCommand` already carries: issuer, source entities, target entity/location, `PayloadName`, `bQueued`, monotonic `CommandId` — enough for M1 local exec and later serialization (`UKodCommandSerializers`).

---

## 5. AIModule events → KoD hooks (later)

| BWAPI callback | When it matters for KoD |
|----------------|-------------------------|
| `onStart` / `onEnd` | Match bootstrap / teardown; faction DA apply |
| `onFrame` | AI policy tick (may be slower than 16 Hz sim) |
| `onUnitShow` / `onUnitHide` | Fog discovery — drive UI pings + AI memory |
| `onUnitCreate` / `onUnitDestroy` / `onUnitComplete` | Production / combat listeners; idle hash invalidation |
| `onUnitMorph` | Transform / siege-like stance changes (if we add them) |
| `onNukeDetect`-class alerts | Superweapon telegraph (Generals flavor) |

Strategy/AI seat should treat this as an **event contract** for a future `UKodAISubsystem`, not a BWAPI subclass.

---

## 6. Type tables ≈ Warden DataAssets

BWAPI’s UnitType / WeaponType / Upgrade catalogs are static encyclopedias bots query every frame.

KoD already chose Primary DataAssets under `/Game/Warden/Data/<Id>.<Id>` (bare ids: `RangerRifle`, `Ranger`, `Dozer`, `CommandCenter`, `Barracks`, `USA`) with AssetManager scan + `UKodSlice0Bootstrap` fallback.

Keep that direction:

- Weapons, units, buildings, factions stay **authored data**.  
- No hardcoded HP / damage in C++ (Slice 0 law).  
- Combat pass: Armor application on hitscan — note only; not BWAPI’s exact formula.

---

## 7. What we explicitly discard

- Injector / Chaoslauncher / multi-instance local PC tricks  
- Brood War `latencyFrames` as our net model (design UE replication / lockstep later from ZH *ideas* + modern RTS practice)  
- Bullet arrays as actor projectiles for Slice 0 (hitscan)  
- Complete-map-information as a default for competitive modes  
- Cloning BWAPI into `reference/` unless Director asks for a pinned offline mirror (prefer remote headers + this note)

---

## 8. Working practices

1. When “how do SC bots think about orders?” → read BWAPI headers remotely → update **this** note → implement with Kod types.  
2. ZH remains the broader **RTS product** checklist (production, control bar, generals powers). BWAPI is the sharper **AI/command/observe** checklist.  
3. Ping Strategy/AI when expanding `EKodCommandType` or adding an AI tick — Lead owns command/sim contracts.  
4. Slice 0 gate still: editor `L_Slice0` + walk checklist; this doc does **not** change Ops ping rules.

---

## 9. Appendix — high-value upstream reads

| Topic | Upstream path |
|-------|----------------|
| Command enum | `bwapi/include/BWAPI/UnitCommandType.h` |
| AI callbacks | `bwapi/include/BWAPI/AIModule.h` |
| Shared memory snapshot | `bwapi/include/BWAPI/Client/GameData.h` |
| Unit command payload | `bwapi/include/BWAPI/Client/UnitCommand.h` |
| Public site | http://bwapi.github.io/ |
| Repo | https://github.com/bwapi/bwapi |
