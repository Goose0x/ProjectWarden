# Kingdom of Dust - L_FaultlineTraining landscape material, layer infos, and flat landscape (UE 5.8 editor Python)
# Run from the editor, or:
# UnrealEditor-Cmd.exe KingdomOfDust.uproject -run=pythonscript -script="Tools/kod_faultline_landscape_setup.py"
# Creates: /Game/Warden/Environment/Landscape/M_KodLandscape_Faultline (+ MI_), LI_Dirt/LI_Rock/LI_Sand.
# Reuses AAM Painted_Grass_LayerInfo. Then calls UKodLandscapeBuilder.create_faultline_landscape
# (editor module only) for the 505x505 flat landscape at (-11200, -11200, 0).
import unreal

DST = "/Game/Warden/Environment/Landscape"
GRASS_LI = "/Game/Advanced_Auto_Material/Levels/Island_sharedassets/Painted_Grass_LayerInfo"
TILE_UU = 400.0   # world-space UV tile size (2 grid cells); tune in MI
# name -> search keywords for texture lookup (first match wins; override with explicit paths if known)
LAYERS = [
    ("Painted_Grass", ["Painted_Grass", "PaintedGrass"]),
    ("Dirt",          ["Bedrock"]),        # Bedrock basecolor, darker/wet tint in MI
    ("Rock",          ["Bedrock"]),
    ("Sand",          ["Graysand", "GraySand"]),
]
EXPLICIT = {  # fill if search picks wrong: "Rock": ("/Game/.../T_Bedrock_D", "/Game/.../T_Bedrock_N")
}

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
reg = unreal.AssetRegistryHelpers.get_asset_registry()

def log(m): unreal.log("[KodLandscape] " + m)

def find_tex(keys):
    flt = unreal.ARFilter(class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "Texture2D")], recursive_paths=True, package_paths=["/Game"])
    hits = reg.get_assets(flt)
    d = n = None
    for a in hits:
        nm = str(a.asset_name); low = nm.lower()
        if not any(k.lower() in low for k in keys): continue
        p = str(a.package_name)
        if n is None and (low.endswith("_n") or "normal" in low): n = p
        elif d is None and (low.endswith(("_d", "_bc", "_albedo", "_basecolor", "_diffuse")) or "albedo" in low or "basecolor" in low): d = p
    return d, n

def load_tex(path, label):
    if not path:
        log(f"WARN {label} texture not found; leaving it unset")
        return None
    try:
        asset = eal.load_asset(path)
    except Exception as e:
        log(f"WARN {label} texture failed to load {path} ({e}); leaving it unset")
        return None
    if not asset:
        log(f"WARN {label} texture missing at {path}; leaving it unset")
        return None
    return asset

# --- layer infos ---
eal.make_directory(DST)
layer_infos = {}
layer_infos["Painted_Grass"] = eal.load_asset(GRASS_LI)
if not layer_infos["Painted_Grass"]: raise RuntimeError("Missing " + GRASS_LI)
for name in ("Dirt", "Rock", "Sand"):
    path = f"{DST}/LI_{name}"
    if not eal.does_asset_exist(path):
        eal.duplicate_asset(GRASS_LI, path)
    li = eal.load_asset(path)
    # layer_name is read-only from Python. The builder calls SetLayerName for LI_Dirt/Rock/Sand.
    eal.save_asset(path)
    layer_infos[name] = li
grass_layer_name = str(layer_infos["Painted_Grass"].get_editor_property("layer_name"))
log("Grass layer name = " + grass_layer_name)

# --- material ---
mpath = f"{DST}/M_KodLandscape_Faultline"
if eal.does_asset_exist(mpath): eal.delete_asset(mpath)
mat = at.create_asset("M_KodLandscape_Faultline", DST, unreal.Material, unreal.MaterialFactoryNew())
E = lambda cls, x, y: mel.create_material_expression(mat, cls, x, y)

# world-aligned UVs: WorldPosition.xy / TileSize
wp = E(unreal.MaterialExpressionWorldPosition, -1800, 0)
mask = E(unreal.MaterialExpressionComponentMask, -1650, 0)
mask.set_editor_property("r", True); mask.set_editor_property("g", True)
mask.set_editor_property("b", False); mask.set_editor_property("a", False)
mel.connect_material_expressions(wp, "", mask, "")
tile = E(unreal.MaterialExpressionScalarParameter, -1650, 120)
tile.set_editor_property("parameter_name", "TileSizeUU"); tile.set_editor_property("default_value", TILE_UU)
uv = E(unreal.MaterialExpressionDivide, -1500, 40)
mel.connect_material_expressions(mask, "", uv, "A"); mel.connect_material_expressions(tile, "", uv, "B")

prev_c = prev_n = prev_r = None
y = -900
for i, (name, keys) in enumerate(LAYERS):
    d, n = EXPLICIT.get(name, find_tex(keys))
    log(f"{name}: D={d} N={n}")
    lname = grass_layer_name if name == "Painted_Grass" else name
    # color
    sd = E(unreal.MaterialExpressionTextureSampleParameter2D, -1200, y)
    sd.set_editor_property("parameter_name", f"{name}_BaseColor")
    base_tex = load_tex(d, f"{name} base color")
    if base_tex: sd.set_editor_property("texture", base_tex)
    mel.connect_material_expressions(uv, "", sd, "UVs")
    tint = E(unreal.MaterialExpressionVectorParameter, -1200, y + 160)
    tint.set_editor_property("parameter_name", f"{name}_Tint"); tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    mul = E(unreal.MaterialExpressionMultiply, -950, y)
    mel.connect_material_expressions(sd, "RGB", mul, "A"); mel.connect_material_expressions(tint, "", mul, "B")
    # normal
    sn = E(unreal.MaterialExpressionTextureSampleParameter2D, -1200, y + 280)
    sn.set_editor_property("parameter_name", f"{name}_Normal")
    sn.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    normal_tex = load_tex(n, f"{name} normal") if n else None
    if not normal_tex:
        normal_tex = load_tex("/Engine/EngineMaterials/FlatNormal", f"{name} flat normal")
    if normal_tex: sn.set_editor_property("texture", normal_tex)
    mel.connect_material_expressions(uv, "", sn, "UVs")
    rough = E(unreal.MaterialExpressionScalarParameter, -950, y + 280)
    rough.set_editor_property("parameter_name", f"{name}_Roughness"); rough.set_editor_property("default_value", 0.85 if name != "Rock" else 0.7)
    # chained LandscapeLayerWeight (Base = previous, Layer = this)
    outs = []
    for k, (src, pin) in enumerate(((mul, ""), (sn, "RGB"), (rough, ""))):
        w = E(unreal.MaterialExpressionLandscapeLayerWeight, -600 + 200 * k, y + 120 * k)
        w.set_editor_property("parameter_name", lname)
        w.set_editor_property("preview_weight", 1.0 if i == 0 else 0.0)
        prev = (prev_c, prev_n, prev_r)[k]
        if prev: mel.connect_material_expressions(prev, "", w, "Base")
        mel.connect_material_expressions(src, pin, w, "Layer")
        outs.append(w)
    prev_c, prev_n, prev_r = outs
    y += 560

mel.connect_material_property(prev_c, "", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(prev_n, "", unreal.MaterialProperty.MP_NORMAL)
mel.connect_material_property(prev_r, "", unreal.MaterialProperty.MP_ROUGHNESS)
mel.recompile_material(mat)
eal.save_asset(mpath)

mi_path = f"{DST}/MI_KodLandscape_Faultline"
if not eal.does_asset_exist(mi_path):
    mi = at.create_asset("MI_KodLandscape_Faultline", DST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, mat)
    # Dirt reads distinct from Rock: darker warm tint
    mel.set_material_instance_vector_parameter_value(mi, "Dirt_Tint", unreal.LinearColor(0.55, 0.45, 0.38, 1))
    eal.save_asset(mi_path)
layer_paths = [
    GRASS_LI,
    f"{DST}/LI_Dirt",
    f"{DST}/LI_Rock",
    f"{DST}/LI_Sand",
]
actor = unreal.KodLandscapeBuilder.create_faultline_landscape(mi_path, layer_paths)
for name in ("Dirt", "Rock", "Sand"):
    eal.save_asset(f"{DST}/LI_{name}")
log("Landscape = " + str(actor))
log("DONE. Material + MI + layer infos saved under " + DST)
