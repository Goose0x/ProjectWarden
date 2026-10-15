"""UE 5 EDITOR script - Stylized Crystals delta (2026-10-14). Run on NAOMI on a WORK COPY level.

    MODE = "bounds" : measures SM_Crystal_Cluster_1-12, SM_Crystal_1-6, _Broken_1-6, SM_Crystal_Shard and writes
                  crystal_bounds.json {path: {x,y,z}} next to this script. Copy it to the box at
                  /workspace/artifacts/maps/02_concepts/_shared/crystal_bounds.json and re-run the generators:
                  the Jadeite pool then becomes "clusters >= 60 uu wide" with exact per-mesh scale (190 / max XY).
    MODE = "mi"     : creates /Game/Kod/Proxy/Materials/MI_Jadeite_Node (parent M_Crystal) and MI_Jadeite_Node_Rich
                  (parent MI_Jadeite_Node) if missing. Sets NO colour params (names unknown until opened): log lists
                  the parent's vector/scalar params so Art can set the provisional jade.
    MODE = "check"  : loads every path used by the delta and fails on any missing / banned (Peridot, Turquoise).
Changes nothing else. No level actors are touched.
"""
# Patched by ^MODE = "[^"]+"$ only. Docstring examples are indented so they do not match.
import json, os, unreal

MODE = "bounds"
PACK = "/Game/StylizedCrystalsRocksAndGems"
CLUSTERS = [PACK + "/Mesh/Clusters/SM_Crystal_Cluster_%d" % i for i in range(1, 13)]
CRYSTALS = [PACK + "/Mesh/Crystals/SM_Crystal_%d" % i for i in range(1, 7)] + \
           [PACK + "/Mesh/Crystals/SM_Crystal_Broken_%d" % i for i in range(1, 7)] + [PACK + "/Mesh/Crystals/SM_Crystal_Shard"]
M_CRYSTAL = PACK + "/Materials/M_Crystal"
MIS = [PACK + "/Materials/Crystals/MI_Crystal_" + k for k in ("Amber", "Citrine", "Ruby", "Amethyst")]
DEST = "/Game/Kod/Proxy/Materials"
EAL = unreal.EditorAssetLibrary
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "crystal_bounds.json")


def bounds():
    res = {}
    for p in CLUSTERS + CRYSTALS:
        if not EAL.does_asset_exist(p):
            unreal.log_warning("[crystal] MISSING " + p); continue
        bb = EAL.load_asset(p).get_bounding_box(); e = bb.max - bb.min
        res[p] = {"x": round(e.x, 1), "y": round(e.y, 1), "z": round(e.z, 1)}
        w = max(e.x, e.y)
        tag = ("JADE-ELIGIBLE s=%.2f -> h=%.0f" % (190.0 / w, e.z * 190.0 / w)) if p in CLUSTERS and w >= 60 else ""
        unreal.log("[crystal] %-75s %6.1f x %6.1f x %6.1f  %s" % (p, e.x, e.y, e.z, tag))
    json.dump(res, open(OUT, "w"), indent=1)
    unreal.log("[crystal] wrote " + OUT)


def mi():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    parent = EAL.load_asset(M_CRYSTAL)
    for name, par_path in (("MI_Jadeite_Node", M_CRYSTAL), ("MI_Jadeite_Node_Rich", DEST + "/MI_Jadeite_Node")):
        path = DEST + "/" + name
        if EAL.does_asset_exist(path):
            unreal.log("[crystal] exists " + path); continue
        mic = tools.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        unreal.MaterialEditingLibrary.set_material_instance_parent(mic, EAL.load_asset(par_path))
        EAL.save_asset(path); unreal.log("[crystal] created %s (parent %s)" % (path, par_path))
    mel = unreal.MaterialEditingLibrary
    unreal.log("[crystal] M_Crystal vector params: %s" % mel.get_vector_parameter_names(parent))   # VERIFY fn availability on 5.x
    unreal.log("[crystal] M_Crystal scalar params: %s" % mel.get_scalar_parameter_names(parent))


def check():
    bad = 0
    for p in CLUSTERS + CRYSTALS + MIS + [M_CRYSTAL]:
        if any(b in p for b in ("Peridot", "Turquoise")):
            unreal.log_error("[crystal] BANNED " + p); bad += 1
        if not EAL.does_asset_exist(p):
            unreal.log_error("[crystal] MISSING " + p); bad += 1
    unreal.log("[crystal] check: %d problems" % bad)


{"bounds": bounds, "mi": mi, "check": check}[MODE]()
