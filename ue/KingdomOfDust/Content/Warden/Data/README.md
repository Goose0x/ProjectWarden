# Warden Data — Slice 0

**Law:** bare asset ids only (no `DA_` prefix). All six live under `/Game/Warden/Data/`.

| Id | Class |
|----|-------|
| `RangerRifle` | `UKodWeaponDefinition` |
| `Ranger` | `UKodUnitDefinition` |
| `Dozer` | `UKodUnitDefinition` |
| `CommandCenter` | `UKodBuildingDefinition` |
| `Barracks` | `UKodBuildingDefinition` |
| `USA` | `UKodFactionDefinition` |

## Editor steps (when UE is open)

1. Ensure `DefaultGame.ini` AssetManager scans `/Game/Warden/Data` for the four Primary Asset Types.
2. Content Browser → `/Game/Warden/Data`.
3. Right-click → **Miscellaneous → Data Asset** → pick class → **Save As exact id** (`RangerRifle`, not `DA_Weapon_RangerRifle`).
4. Fill fields from the matching `*.md` stub / `WardenSlice0Catalog.json`.
5. Soft refs between assets use the same bare ids.

## Without editor

`UKodSlice0Bootstrap` `NewObject`s the six ids, fills the shop sheet, registers in `UKodDataCatalog`. PIE resolves SoftObjectPath first; on miss, uses bootstrap. When real `.uasset`s land, bootstrap yields.

**Do not create fake binary `.uasset` files in git.**

## BuildTicks

`BuildTicks = RoundToInt(Seconds * 16)` — Ranger 80, Dozer 128, Barracks 320.
