# USA Engineer torch anim lock (v1)

**Status: PASS / AUTHORITATIVE torch v1.**  
**QA PASS:** 2026-09-28 ~21:35 PT  
**Scope:** Grounded Engineer loco and torch holds. Import **torch_v1b** byte sizes only.

This is the Soft Dev ship lock. Do not commit `.fbx`, previews, or other binary anim assets.

Slice 0 still uses the Ranger PROXY. Do not swap Slice 0 to Engineer or Recon.

## Drive pins

Import FBXs from **04_anims** only:

https://drive.google.com/drive/folders/1HyzSEqiiVICvBxFrJIQGkoi5mtjW6-Q7

Pack folder (context, not a second anim source):

https://drive.google.com/drive/folders/1Kl_l83E73s34_bSyZalJF3E8ATVJYYs4

The 04_anims folder may still contain older twin files. Match **byte size** before import. Name match is not enough.

## Byte-size gate (torch_v1b)

| File | Bytes |
|------|------:|
| `US_Infantry_Engineer_anim_Idle.fbx` | 616492 |
| `US_Infantry_Engineer_anim_Walking.fbx` | 582876 |
| `US_Infantry_Engineer_anim_Running.fbx` | 558044 |
| `US_Infantry_Engineer_anim_Aim.fbx` | 566332 |
| `US_Infantry_Engineer_anim_Fire.fbx` | 560684 |
| `US_Infantry_Engineer_anim_Hit_Reaction.fbx` | 608252 |
| `US_Infantry_Engineer_anim_Fall_Down.fbx` | 748444 |
| `ANIM_NOTES.txt` | 3748 |
| `RETARGET_REPORT.json` | 7195 |

## Reject

Do not import twins that share the clip name and fail the size gate:

- Older Idle twin **616652** bytes
- Older notes twin **3246** bytes
- Older report twin **8137** bytes
- The older preview set sitting beside those twins

## Editor import

When Engineer is mounted, import the seven clips from Drive `1HyzSEqiiVICvBxFrJIQGkoi5mtjW6-Q7` only after each file matches the torch_v1b size above. Do not drop the FBXs into git.

Slice 0 walk, train, and spawn stay on the Ranger PROXY (`Ranger` under `/Game/Warden/Data/`). See [04-slice0-warden.md](04-slice0-warden.md). Recon hover stays on its own lock: [06-usa-recon-anim-lock.md](06-usa-recon-anim-lock.md).

## Pose lock

- **Loco** — grounded Ranger (feet near the ground). Not hover
- **Idle / Walk / Run** — hip carry. Right hand stays down (not arms-up)
- **Aim / Fire** — two-hand torch forward (Idle→Aim Δ rhH ≈ **0.472**)
- **Torch** — parented to `RightHand` on every clip, including Hit and Fall

## Ship clips

| Clip | File |
|------|------|
| Idle | `US_Infantry_Engineer_anim_Idle.fbx` |
| Walking | `US_Infantry_Engineer_anim_Walking.fbx` |
| Running | `US_Infantry_Engineer_anim_Running.fbx` |
| Aim | `US_Infantry_Engineer_anim_Aim.fbx` |
| Fire | `US_Infantry_Engineer_anim_Fire.fbx` |
| Hit | `US_Infantry_Engineer_anim_Hit_Reaction.fbx` |
| Fall | `US_Infantry_Engineer_anim_Fall_Down.fbx` |
