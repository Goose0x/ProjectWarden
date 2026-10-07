# USA Ranger body — local import

Slice 0 static mesh mount. Do not commit `.uasset`, `.umap`, or FBX files.

| | Soft path |
|--|-----------|
| Folder | `/Game/Warden/Characters/USA/Ranger/` |
| Static mesh | `/Game/Warden/Characters/USA/Ranger/SM_Ranger_Body.SM_Ranger_Body` |
| DataAsset | `/Game/Warden/Data/Ranger.Ranger` (`StaticMesh` soft ref) |

Rename the imported static mesh to `SM_Ranger_Body` if Import keeps the source file name. Leave **Skeletal Mesh** empty until anims land. Bootstrap leaves the ref null, so a missing asset stays the Engine cube.

Steps: `docs/04-slice0-warden.md` (Ranger body mesh).
