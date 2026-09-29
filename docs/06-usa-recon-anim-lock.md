# USA Recon hover anim lock (v2.1)

**QA PASS:** 2026-09-28 ~21:05 PT  
**Scope:** T1/T2 hover loco and combat holds for USA Infantry Recon.  
**Not in this lock:** Slice 0 presentation. Slice 0 still uses the Ranger PROXY. Do not swap Slice 0 to Recon yet.

This is a docs pin only. Do not commit `.fbx`, previews, Mixamo donors, or other binary anim assets.

## Drive pin

Import from this folder only when mounting Recon:

https://drive.google.com/drive/folders/1pStr5gDAzGkdSZSztBPuYaYJdP0yYgJe

## Reject

Do not use these as ship loco:

- Parent `04_anims` grounded leftovers
- Older hover_v2 folder `14lRuC6mN9POdDj0mdYj_1S6evLfSYNjI`
- Mixamo donor FBXs (`mixamo_Floating`, `mixamo_Flying`, `mixamo_Fly_Idle`, `mixamo_Falling_Idle`)

Grounded carbine patrol is not the hover set. Hover v2 Idle sat too close to Aim; v2.1 is the contrast fix.

## Editor import

When Recon is mounted, import the seven ship clips below from the Drive folder above. Do not import a local parent-tree copy, and do not drop the FBXs into git.

Slice 0 walk, train, and spawn stay on the Ranger PROXY (`Ranger` under `/Game/Warden/Data/`). See [04-slice0-warden.md](04-slice0-warden.md).

## Pose lock

- **Idle** — upright Floating hover, lower-torso muzzle-down low-ready (rhH ≈ 0.438, muzzle_z ≈ −0.484)
- **Walk / Run** — Superman aero (upright ≈ 0.21). Walk is root-locked (world XZ held to frame 0; hover height and bob stay)
- **Aim / Fire** — cheek weld, muzzle about horizontal (Aim rhH ≈ 0.804, muzzle_z ≈ −0.023)
- **Idle → Aim** — Δ rhH ≈ **0.366** (gate was ≥ 0.20)
- **SMG** — parented to `RightHand` on every clip, including Hit and Fall
- **Hit** — low-ready (not cheek weld)
- **Fall_Down** — Ranger collapse retained (a ground fall, not mid-air Falling_Idle)

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
