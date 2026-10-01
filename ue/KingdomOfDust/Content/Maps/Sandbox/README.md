# Maps / Sandbox

## L_Slice0 (walk gate)

Create in editor: **File → New Level** (empty/open world greybox) → Save As  
`/Game/Maps/Sandbox/L_Slice0`

World Settings / GameMode Override: `/Script/KingdomOfDust.KodSlice0GameMode` (`AKodSlice0GameMode`, or a BP child of it).

Not `/Script/KoD_alpha.KodSlice0GameMode`. The `.uproject` file name is not the module. Open `ue/KingdomOfDust/KingdomOfDust.uproject`. A flat `KoD_alpha.uproject` must list the same Modules as `KingdomOfDust.uproject`.

Preplaced PROXY (when meshes exist): 1× CommandCenter, 1× Barracks, 1× Dozer, 4× Ranger.  
Placed greybox units are native `AKodProxyUnit` (`/Script/KodUnits.KodProxyUnit`), a `AKodUnit` with default subobject `ProxyBody`. That class has to exist or `L_Slice0` logs `CreateExport: Failed to load Outer` for Arrow, CollisionCylinder, CharMoveComp, CharacterMesh0, and ProxyBody.  
Until then, GameMode `bSpawnSmokeRanger` spawns one Ranger from bootstrap for move/hash smoke.

If PIE logged `Failed to find Class /Script/KoD_alpha.KodSlice0GameMode` or `KodProxyUnit_*` CreateExport Outer warnings, set the GameMode path above, then delete and re-place those PROXY actors.

No fake `.umap` in git.
