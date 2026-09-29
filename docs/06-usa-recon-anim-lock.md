# USA Recon hover anim lock (v2.3)

**Status: PASS / AUTHORITATIVE hover v2.3.**  
**QA PASS:** 2026-09-28 ~21:23 PT  
**Scope:** T1/T2 hover loco and combat holds for USA Infantry Recon.

This is the Soft Dev ship lock. Import from the Drive folder below only when mounting Recon. Do not commit `.fbx`, previews, Mixamo donors, or other binary anim assets.

Slice 0 still uses the Ranger PROXY. Do not swap Slice 0 to Recon yet.

## Drive pin

https://drive.google.com/drive/folders/1J2iypSWZsuXkG2DiraRX55WQ5cEqBXz3

Folder id: `1J2iypSWZsuXkG2DiraRX55WQ5cEqBXz3`

## Reject

Do not import:

- Grounded parent `04_anims` leftovers
- v2.1 folder `1pStr5gDAzGkdSZSztBPuYaYJdP0yYgJe` (Director-rejected after its QA pass: muzzle facing wrong, Walk/Run full Superman prone fly)
- v2.2 folder `1dcYJXuu8M1S4R493lfS5G4pTtDaVRqW-` (Run lean over 45°)
- Any interim Run lean-only patches
- Older hover_v2 folder `14lRuC6mN9POdDj0mdYj_1S6evLfSYNjI`
- Mixamo donor FBXs

Those folders are a reject trail. They are not the current lock.

## Editor import

When Recon is mounted, import the seven ship clips below from the v2.3 Drive folder only. Do not drop the FBXs into git.

Slice 0 walk, train, and spawn stay on the Ranger PROXY (`Ranger` under `/Game/Warden/Data/`). See [04-slice0-warden.md](04-slice0-warden.md).

## Pose lock

- **Idle** — upright Floating hover plus cross-body SMG carry (right hand near the hip, not arms-up)
- **Walk** — about **42°** forward lean (stamped ≈ 41.8°). Pitch is forward, not falling back. Not Superman prone
- **Run** — about **45°** forward lean (stamped ≈ 44.8°). **≤ 45° tops**
- **Aim / Fire** — cheek weld. Idle→Aim Δ rhH ≈ **0.449**
- **Muzzle** — **+Y** forward on every clip
- **SMG** — parented to `RightHand` on every clip, including Hit and Fall
- **Hit** — cross-body carry (not cheek weld)
- **Fall_Down** — Ranger collapse retained (a ground fall, not mid-air hover)

## Ship clips

| Clip | File |
|------|------|
| Idle | `US_Infantry_Recon_anim_Idle.fbx` |
| Walking | `US_Infantry_Recon_anim_Walking.fbx` |
| Running | `US_Infantry_Recon_anim_Running.fbx` |
| Aim | `US_Infantry_Recon_anim_Aim.fbx` |
| Fire | `US_Infantry_Recon_anim_Fire.fbx` |
| Hit | `US_Infantry_Recon_anim_Hit_Reaction.fbx` |
| Fall | `US_Infantry_Recon_anim_Fall_Down.fbx` |
