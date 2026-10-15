# Kingdom of Dust — wireframe silhouette material (UE 5.8 editor Python, headless).
# UnrealEditor-Cmd.exe KingdomOfDust.uproject -run=pythonscript -script="Tools/kod_wireframe_mat_setup.py"
#
# Creates /Game/Warden/UI/Wireframes/M_KodWireSilhouette (unlit, translucent).
# Does not commit a .uasset. C++ draws generated zone shapes until this material
# and a silhouette texture both exist.
#
# Parameters the HUD writes on a dynamic instance, one instance per silhouette
# each frame so a group grid can show a different HP on every cell:
# The silhouette texture is the unit's own icon, drawn at 160 or 48 by the HUD.
#   Silhouette          icon texture
#   ZoneMask            optional. Units: R head, G torso, B legs, A weapon.
#                       Buildings: R roof, G body, B base. Empty mask uses ZoneTint0
#                       for the whole icon (overall HP).
#   ZoneRemain0..3      scalar 0..1
#   ZoneTint0..3        HUD HP tokens
#   ShieldRim           0 hides the rim. Above 0 when the unit has shields.
#   ShieldRimColor      blue rim
#
# Bands: HP.Green #4CD964 at >= 66%, HP.Yellow #F2C230 from 33% up to 66%,
# HP.Red #E5383B below 33%. Do not use the placement colours #3DFF6A / #FF3D3D.
#
# TextureSample's RGB pin is float3. SM6 rejects `Mask.a` on that pin
# ("vector swizzle 'a' is out of bounds"). Append RGB with the separate A pin
# so the Custom input Mask is float4 before any .a swizzle. Re-run this
# script in the editor; the .uasset is not committed.
import unreal

DST = "/Game/Warden/UI/Wireframes"
NAME = "M_KodWireSilhouette"
HLSL = """
// Mask is the AppendVector of TextureSample RGB and A, so it is float4.
float W0 = Mask.r;
float W1 = Mask.g;
float W2 = Mask.b;
float W3 = Mask.a;
float MaskSum = W0 + W1 + W2 + W3;
float3 Tint = T0.rgb;
if (MaskSum > 1e-3)
{
    float3 Acc = float3(0.0, 0.0, 0.0);
    float Weight = 0.0;
    float w;
    w = W0 * R0; Acc += T0.rgb * w; Weight += w;
    w = W1 * R1; Acc += T1.rgb * w; Weight += w;
    w = W2 * R2; Acc += T2.rgb * w; Weight += w;
    w = W3 * R3; Acc += T3.rgb * w; Weight += w;
    Tint = (Weight > 1e-3) ? Acc / Weight : T0.rgb;
}
float Shade = dot(Silhouette, float3(0.30, 0.59, 0.11));
Shade = lerp(0.45, 1.0, saturate(Shade));
float Edge = ShieldRim * saturate(fwidth(SilhouetteA) * 8.0);
float3 Color = Tint * Shade + ShieldColor.rgb * Edge;
float Cover = max(SilhouetteA, Edge);
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
    # HP.Green #4CD964 in linear. The HUD overwrites ZoneTint each draw.
    expr.set_editor_property("default_value", unreal.LinearColor(0.072, 0.694, 0.127, 1.0))
    return expr


log("creating " + NAME)
material = recreate()
material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)

silhouette = texture_param(material, "Silhouette", -900, -200)
zone_mask = texture_param(material, "ZoneMask", -900, 80)
custom = mel.create_material_expression(material, unreal.MaterialExpressionCustom, -200, 0)
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT4)
custom.set_editor_property("description", "WireZones")
custom.set_editor_property("code", HLSL)
pins = []
for pin_name in (
    "Silhouette", "SilhouetteA", "Mask",
    "R0", "R1", "R2", "R3",
    "T0", "T1", "T2", "T3",
    "ShieldRim", "ShieldColor",
):
    pin = unreal.CustomInput()
    pin.set_editor_property("input_name", pin_name)
    pins.append(pin)
custom.set_editor_property("inputs", pins)
mask4 = mel.create_material_expression(material, unreal.MaterialExpressionAppendVector, -520, 80)
mel.connect_material_expressions(zone_mask, "RGB", mask4, "A")
mel.connect_material_expressions(zone_mask, "A", mask4, "B")
mel.connect_material_expressions(silhouette, "RGB", custom, "Silhouette")
mel.connect_material_expressions(silhouette, "A", custom, "SilhouetteA")
mel.connect_material_expressions(mask4, "", custom, "Mask")
for index in range(4):
    remain = scalar_param(material, f"ZoneRemain{index}", -600, -40 + index * 40)
    tint = vector_param(material, f"ZoneTint{index}", -600, 220 + index * 40)
    mel.connect_material_expressions(remain, "", custom, f"R{index}")
    mel.connect_material_expressions(tint, "", custom, f"T{index}")
shield = scalar_param(material, "ShieldRim", -600, 420)
shield.set_editor_property("default_value", 0.0)
rim_color = vector_param(material, "ShieldRimColor", -600, 480)
rim_color.set_editor_property("default_value", unreal.LinearColor(0.30, 0.65, 1.0, 1.0))
mel.connect_material_expressions(shield, "", custom, "ShieldRim")
mel.connect_material_expressions(rim_color, "", custom, "ShieldColor")
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
