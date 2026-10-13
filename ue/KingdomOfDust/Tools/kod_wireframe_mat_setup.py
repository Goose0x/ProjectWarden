# Kingdom of Dust — wireframe silhouette material (UE 5.8 editor Python, headless).
# UnrealEditor-Cmd.exe KingdomOfDust.uproject -run=pythonscript -script="Tools/kod_wireframe_mat_setup.py"
#
# Creates /Game/Warden/UI/Wireframes/M_KodWireSilhouette (unlit, translucent).
# Does not commit a .uasset. C++ draws generated zone shapes until this material
# and a silhouette texture both exist.
#
# Parameters the HUD writes on a dynamic instance, one instance per silhouette
# each frame so a group grid can show a different HP on every cell:
#   Silhouette          texture, the unit wireframe (128 authored, scaled by the HUD)
#   ZoneMaskA           rgba weights for zones 0-3. Black falls back to ZoneTint0.
#   ZoneMaskB           rg weights for zones 4-5.
#   ZoneRemain0..5      scalar 0..1. Today every zone is overall HP.
#   ZoneTint0..5        linear colour. Today every zone is the same HP band.
#
# Bands: #3DFF6A above 66%, #FFD23D from 33% through 66%, #FF3D3D below 33%.
import unreal

DST = "/Game/Warden/UI/Wireframes"
NAME = "M_KodWireSilhouette"
HLSL = """
float W0 = MaskA.r;
float W1 = MaskA.g;
float W2 = MaskA.b;
float W3 = MaskA.a;
float W4 = MaskB.r;
float W5 = MaskB.g;
float MaskSum = W0 + W1 + W2 + W3 + W4 + W5;
float3 Color = T0.rgb;
float Cover = Silhouette.a;
if (MaskSum > 1e-3)
{
    float3 Acc = float3(0.0, 0.0, 0.0);
    float Weight = 0.0;
    float w;
    w = W0 * R0; Acc += T0.rgb * w; Weight += w;
    w = W1 * R1; Acc += T1.rgb * w; Weight += w;
    w = W2 * R2; Acc += T2.rgb * w; Weight += w;
    w = W3 * R3; Acc += T3.rgb * w; Weight += w;
    w = W4 * R4; Acc += T4.rgb * w; Weight += w;
    w = W5 * R5; Acc += T5.rgb * w; Weight += w;
    Color = (Weight > 1e-3) ? Acc / Weight : T0.rgb;
}
return float4(Color, Cover);
"""

at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(message):
    unreal.log("[KodWire] " + message)


def recreate():
    path = f"{DST}/{NAME}"
    if eal.does_asset_exist(path):
        eal.delete_asset(path)
    eal.make_directory(DST)
    return at.create_asset(NAME, DST, unreal.Material, unreal.MaterialFactoryNew())


def texture_param(material, name, x, y):
    expr = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    expr.set_editor_property("parameter_name", name)
    return expr


def scalar_param(material, name, x, y):
    expr = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("default_value", 1.0)
    return expr


def vector_param(material, name, x, y):
    expr = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    expr.set_editor_property("parameter_name", name)
    expr.set_editor_property("default_value", unreal.LinearColor(0.24, 1.0, 0.42, 1.0))
    return expr


log("creating " + NAME)
material = recreate()
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)

silhouette = texture_param(material, "Silhouette", -900, -200)
mask_a = texture_param(material, "ZoneMaskA", -900, 80)
mask_b = texture_param(material, "ZoneMaskB", -900, 280)
custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -200, 0)
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
custom.set_editor_property("description", "WireZones")
custom.set_editor_property("code", HLSL)
pins = []
for pin_name in (
    "Silhouette", "MaskA", "MaskB",
    "R0", "R1", "R2", "R3", "R4", "R5",
    "T0", "T1", "T2", "T3", "T4", "T5",
):
    pin = unreal.CustomInput()
    pin.set_editor_property("input_name", pin_name)
    pins.append(pin)
custom.set_editor_property("inputs", pins)
mel.connect_material_expressions(silhouette, "", custom, "Silhouette")
mel.connect_material_expressions(mask_a, "", custom, "MaskA")
mel.connect_material_expressions(mask_b, "", custom, "MaskB")
for index in range(6):
    remain = scalar_param(material, f"ZoneRemain{index}", -600, -40 + index * 40)
    tint = vector_param(material, f"ZoneTint{index}", -600, 220 + index * 40)
    mel.connect_material_expressions(remain, "", custom, f"R{index}")
    mel.connect_material_expressions(tint, "", custom, f"T{index}")
rgb = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, 80, -40)
rgb.set_editor_property("R", True)
rgb.set_editor_property("G", True)
rgb.set_editor_property("B", True)
rgb.set_editor_property("A", False)
mel.connect_material_expressions(custom, "", rgb, "")
mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
opacity = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, 80, 80)
opacity.set_editor_property("R", False)
opacity.set_editor_property("G", False)
opacity.set_editor_property("B", False)
opacity.set_editor_property("A", True)
mel.connect_material_expressions(custom, "", opacity, "")
mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
mel.recompile_material(material)
eal.save_asset(f"{DST}/{NAME}")
log("DONE " + DST + "/" + NAME)
