# Maps / Sandbox

## L_Slice0 (walk gate)

Create in editor: **File → New Level** (empty/open world greybox) → Save As  
`/Game/Maps/Sandbox/L_Slice0`

World Settings / GameMode Override: `AKodSlice0GameMode` (or BP child).

Preplaced PROXY (when meshes exist): 1× CommandCenter, 1× Barracks, 1× Dozer, 4× Ranger.  
Placed greybox units are native `AKodProxyUnit` (`/Script/KodUnits.KodProxyUnit`), a `AKodUnit` with default subobject `ProxyBody`. That class has to exist or `L_Slice0` logs `CreateExport: Failed to load Outer` for Arrow, CollisionCylinder, CharMoveComp, CharacterMesh0, and ProxyBody.  
Until then, GameMode `bSpawnSmokeRanger` spawns one Ranger from bootstrap for move/hash smoke.

No fake `.umap` in git.
