# USA Recon hover anim — v2.1 HOLD

**Status: HOLD — Director rejected v2.1 after QA PASS.**  
**Not ship-final.** Do not import this package as Recon loco. Art must rework it and QA must re-stamp before Soft Dev treats any folder as authoritative.

QA had passed v2.1 on 2026-09-28 ~21:05 PT. The Director rejected it after that pass. This file keeps the rejected clip names and the pose history so the next stamp can supersede them. It is not a ship lock.

Slice 0 still uses the Ranger PROXY. Do not swap Slice 0 to Recon. Do not commit `.fbx`, previews, Mixamo donors, or other binary anim assets.

## Not authoritative

Drive folder `1pStr5gDAzGkdSZSztBPuYaYJdP0yYgJe` is **not** the ship source until Art reworks the clips and QA re-stamps:

https://drive.google.com/drive/folders/1pStr5gDAzGkdSZSztBPuYaYJdP0yYgJe

Also do not use:

- Parent `04_anims` grounded leftovers
- Older hover_v2 folder `14lRuC6mN9POdDj0mdYj_1S6evLfSYNjI`
- Mixamo donor FBXs

## Director fails on v2.1

1. **SMG muzzle / facing is the wrong way.**
2. **Walk / Run is full Superman aero** (prone fly, upright ≈ 0.21). Director wants **~45° pitch max**, not a prone fly.

## Rejected package (history only)

These names and poses are the rejected v2.1 set. They are superseded pending the next stamp. Do not mount them.

| Clip | File |
|------|------|
| Idle | `US_Infantry_Recon_anim_Idle.fbx` |
| Walking | `US_Infantry_Recon_anim_Walking.fbx` |
| Running | `US_Infantry_Recon_anim_Running.fbx` |
| Aim | `US_Infantry_Recon_anim_Aim.fbx` |
| Fire | `US_Infantry_Recon_anim_Fire.fbx` |
| Hit | `US_Infantry_Recon_anim_Hit_Reaction.fbx` |
| Fall | `US_Infantry_Recon_anim_Fall_Down.fbx` |

Recorded v2.1 poses (rejected, not a target to match):

- Idle was upright Floating with lower-torso muzzle-down low-ready (rhH ≈ 0.438, muzzle_z ≈ −0.484)
- Walk / Run were Superman aero; Walk was root-locked
- Aim / Fire were cheek weld, muzzle about horizontal (Idle→Aim Δ rhH ≈ 0.366)
- SMG was parented to `RightHand` on every clip
- Fall_Down kept the Ranger collapse

## Slice 0

Walk, train, and spawn stay on the Ranger PROXY (`Ranger` under `/Game/Warden/Data/`). See [04-slice0-warden.md](04-slice0-warden.md).
