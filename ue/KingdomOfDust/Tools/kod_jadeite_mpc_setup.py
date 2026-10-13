# Kingdom of Dust — Jadeite colour collection (UE 5.8 editor Python, headless).
# UnrealEditor-Cmd.exe KingdomOfDust.uproject -run=pythonscript -script="Tools/kod_jadeite_mpc_setup.py"
#
# Creates, under /Game/Warden/Materials:
#   MPC_KodJadeite     parameter collection
#   MF_KodJadeiteTint  reads that collection and tints one colour
#   M_KodJadeite       Jadeite node material (Base + Emissive already hooked)
#
# MPC parameters:
#   JadeHueShift, JadeSaturation, JadeBrightness, JadeEmissive
#   JadeShadow #0B4F5C, JadeBase #11928F, JadeLit #2DBFB8, JadeRim #8EEAE4, JadeCore #22CDBE
#
# Where to hook MF_KodJadeiteTint (no inputs; it reads the collection):
#   Base     -> base colour of a Jadeite node. M_KodJadeite already does this.
#   Emissive -> emissive of a Jadeite node, or a Scinder eye / regen glow.
#   Shadow, Lit, Rim, Core are the other tuned roles if a graph needs them.
# The in-game F9 panel writes the four scalars into the collection, so a hooked
# material updates in PIE without a rebuild. F10 is the pause menu.
# Default JadeBase is #11928F. Code tints (node stand-in, carried crystal) read
# that tuned colour. If a node mesh already uses
# /Game/Warden/Environment/Materials/MI_Jadeite_Node (parent M_Crystal), C++
# does not put a dynamic tint on top. Hook Base on M_Crystal so the slider
# drives that material through the collection.
# If a Scinder material still exposes a vector named with Jade, or EyeEmissive,
# EyeGlow, EyeColor, RegenGlow, or RegenEmissive, the tuner also writes the
# tuned colour into that parameter. Remove the parameter once the function
# output is connected, or the parameter override wins over the function.
import unreal

DST = "/Game/Warden/Materials"
TINT_HLSL = """
float MaxC = max(Color.r, max(Color.g, Color.b));
float Delta = MaxC - min(Color.r, min(Color.g, Color.b));
float Hue = 0.0;
float Sat = (MaxC <= 0.0) ? 0.0 : Delta / MaxC;
float Val = MaxC;
if (Delta > 1e-5)
{
    if (MaxC == Color.r) Hue = (Color.g - Color.b) / Delta;
    else if (MaxC == Color.g) Hue = (Color.b - Color.r) / Delta + 2.0;
    else Hue = (Color.r - Color.g) / Delta + 4.0;
    Hue = frac(Hue / 6.0 + 1.0);
}
Hue = frac(Hue + HueShift);
Sat = saturate(Sat * Saturation);
Val = saturate(Val * Brightness);
float H6 = Hue * 6.0;
float Sector = floor(H6);
float Fr = H6 - Sector;
float P = Val * (1.0 - Sat);
float Q = Val * (1.0 - Fr * Sat);
float T = Val * (1.0 - (1.0 - Fr) * Sat);
if (Sector < 1.0) return float3(Val, T, P);
if (Sector < 2.0) return float3(Q, Val, P);
if (Sector < 3.0) return float3(P, Val, T);
if (Sector < 4.0) return float3(P, Q, Val);
if (Sector < 5.0) return float3(T, P, Val);
return float3(Val, P, Q);
"""

def srgb_channel(byte):
    value = byte / 255.0
    if value <= 0.04045:
        return value / 12.92
    return ((value + 0.055) / 1.055) ** 2.4

def srgb(red, green, blue):
    # Same conversion as FLinearColor(FColor). Copy-hex uses ToFColor and expects this.
    return unreal.LinearColor(srgb_channel(red), srgb_channel(green), srgb_channel(blue), 1.0)

SCALARS = (
    ("JadeHueShift", 0.0),
    ("JadeSaturation", 1.0),
    ("JadeBrightness", 1.0),
    ("JadeEmissive", 1.0),
)
VECTORS = (
    ("JadeShadow", srgb(0x0B, 0x4F, 0x5C)),
    ("JadeBase", srgb(0x11, 0x92, 0x8F)),
    ("JadeLit", srgb(0x2D, 0xBF, 0xB8)),
    ("JadeRim", srgb(0x8E, 0xEA, 0xE4)),
    ("JadeCore", srgb(0x22, 0xCD, 0xBE)),
)

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary

def log(message):
    unreal.log("[KodJadeite] " + message)

def recreate(name, asset_class, factory):
    path = f"{DST}/{name}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    eal.make_directory(DST)
    return at.create_asset(name, DST, asset_class, factory)

# 5.8 Python rejects Collection parameter Id and parameter_id writes.
# unreal.GuidLibrary.new_guid() is the guid API; these assets leave Id to the editor.
log("creating MPC_KodJadeite")
mpc = recreate("MPC_KodJadeite", unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
scalars = []
for name, value in SCALARS:
    entry = unreal.CollectionScalarParameter()
    entry.set_editor_property("parameter_name", name)
    entry.set_editor_property("default_value", value)
    scalars.append(entry)
vectors = []
for name, value in VECTORS:
    entry = unreal.CollectionVectorParameter()
    entry.set_editor_property("parameter_name", name)
    entry.set_editor_property("default_value", value)
    vectors.append(entry)
mpc.set_editor_property("scalar_parameters", scalars)
mpc.set_editor_property("vector_parameters", vectors)
eal.save_asset(f"{DST}/MPC_KodJadeite")

def collection_param(owner, name, scalar, x, y, in_function):
    if in_function:
        expr = mel.create_material_expression_in_function(owner, unreal.MaterialExpressionCollectionParameter, x, y)
    else:
        expr = mel.create_material_expression(owner, unreal.MaterialExpressionCollectionParameter, x, y)
    expr.set_editor_property("collection", mpc)
    expr.set_editor_property("parameter_name", name)
    return expr

log("creating MF_KodJadeiteTint")
function = recreate("MF_KodJadeiteTint", unreal.MaterialFunction, unreal.MaterialFunctionFactoryNew())
roles = (
    ("Shadow", "JadeShadow", -40),
    ("Base", "JadeBase", 80),
    ("Lit", "JadeLit", 200),
    ("Rim", "JadeRim", 320),
    ("Core", "JadeCore", 440),
)
hue = collection_param(function, "JadeHueShift", True, -900, -80, True)
sat = collection_param(function, "JadeSaturation", True, -900, 0, True)
bri = collection_param(function, "JadeBrightness", True, -900, 80, True)
glow = collection_param(function, "JadeEmissive", True, -900, 520, True)
tinted = {}
for output_name, vector_name, y in roles:
    color = collection_param(function, vector_name, False, -900, y, True)
    custom = mel.create_material_expression_in_function(function, unreal.MaterialExpressionCustom, -400, y)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    custom.set_editor_property("description", output_name)
    custom.set_editor_property("code", TINT_HLSL)
    inputs = []
    for input_name in ("Color", "HueShift", "Saturation", "Brightness"):
        pin = unreal.CustomInput()
        pin.set_editor_property("input_name", input_name)
        inputs.append(pin)
    custom.set_editor_property("inputs", inputs)
    mel.connect_material_expressions(color, "", custom, "Color")
    mel.connect_material_expressions(hue, "", custom, "HueShift")
    mel.connect_material_expressions(sat, "", custom, "Saturation")
    mel.connect_material_expressions(bri, "", custom, "Brightness")
    out = mel.create_material_expression_in_function(function, unreal.MaterialExpressionFunctionOutput, 0, y)
    out.set_editor_property("output_name", output_name)
    mel.connect_material_expressions(custom, "", out, "")
    tinted[output_name] = custom

emissive = mel.create_material_expression_in_function(function, unreal.MaterialExpressionMultiply, -120, 520)
mel.connect_material_expressions(tinted["Core"], "", emissive, "A")
mel.connect_material_expressions(glow, "", emissive, "B")
emissive_out = mel.create_material_expression_in_function(function, unreal.MaterialExpressionFunctionOutput, 0, 520)
emissive_out.set_editor_property("output_name", "Emissive")
mel.connect_material_expressions(emissive, "", emissive_out, "")
eal.save_asset(f"{DST}/MF_KodJadeiteTint")

log("creating M_KodJadeite")
material = recreate("M_KodJadeite", unreal.Material, unreal.MaterialFactoryNew())
call = mel.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall, -200, 0)
if hasattr(call, "set_material_function"):
    call.set_material_function(function)
else:
    call.set_editor_property("material_function", function)
mel.connect_material_property(call, "Base", unreal.MaterialProperty.MP_BASE_COLOR)
mel.connect_material_property(call, "Emissive", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(material)
eal.save_asset(f"{DST}/M_KodJadeite")
log("DONE. Hook notes are at the top of Tools/kod_jadeite_mpc_setup.py")
