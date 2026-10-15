"""UE 5 EDITOR script for terrain_pass_v2_pcg (run on NAOMI, on L_FaultlineTraining_TPv1 or another work copy).

    MODE = "report"   : measures every pool mesh (bounds height) against the 60 uu walkable cap; logs MISSING assets. Changes nothing.
    MODE = "anchors"  : spawns landmark anchor markers from pcg_anchors_<map>.csv as empty Note actors, tagged
                    KodLandmarkAnchor + LMKIND_<kind> (the only hand-fed points; PCG reads them with Get Actor Data by tag)
    MODE = "volume"   : spawns/updates ONE PCGVolume "PCG_KodTerrainDress" covering the full map, assigns the graph
                    (or graph instance) and seed, Generation Trigger = Generate On Demand. Does not generate.
    MODE = "finalize" : after a manual Generate: per generated ISM/HISM component sets mobility Static,
                    collision off, CanEverAffectNavigation off, cast shadow per pool, cull distances (PERF section)
    MODE = "rollback" : deletes PCG_KodTerrainDress (its generated components go with it) and KodLandmarkAnchor markers.
Nothing here touches KodCliffBlock / KodRamp / KodHighGround / resource / socket actors.
"""
# Patched by ^MODE = "[^"]+"$ only. Docstring examples are indented so they do not match.
import csv, unreal

MODE = "report"
MAP_KEY = "faultline_training"
IMPORT_DIR = r"D:\KoD_Import\k28\terrain"
GRAPH = "/Game/Kod/Env/PCG/PCGI_KodDress_Faultline"     # CREATE (graph instance of PCG_KodDress_Master); VERIFY path once made
SEED = {"faultline_training": 41017, "core_loop_test_map": 62062}[MAP_KEY]
MAP_HALF_UU = {"faultline_training": 11200, "core_loop_test_map": 12800}[MAP_KEY]
CAP_WALK_UU = 60.0
WALK_POOLS = {   # must measure <= 60 uu * max scale
    "P_MICRO_PEBBLE": (["/Game/StylizedOrcWarcamp/StaticMeshes/Rocks/SM_Pebble%s" % c for c in "ABC"] +
                       ["/Game/StylizedRocks_Etherion/RocksVol1/Meshes/GroundRocks/SM_GroundRock_Tiny_%s" % c for c in "ABCDE"], 1.0),
    "P_FLOOR_SHARDS": (["/Game/ThePilgrimsPath/Meshes/SM_BrokenVase"] +
                       ["/Game/Stylized_Egypt/Meshes/props/SM_jug_broken_0%d" % i for i in (1, 2, 3, 4)], 1.0),
}
# per-pool render settings (PERF): (cast_shadow, cull_start, cull_end) in uu; 0 = never cull
RENDER = {"SM_Pebble": (False, 4500, 6000), "SM_GroundRock_Tiny": (False, 4500, 6000), "SM_BrokenVase": (False, 5000, 6500),
          "SM_jug_broken": (False, 5000, 6500), "SM_Bush_": (False, 7000, 9000), "SM_GroundRock_Small": (False, 7000, 9000),
          "SM_GroundRock_Medium": (True, 0, 0), "SM_Boulder": (True, 0, 0), "SM_Debris": (False, 7000, 9000), "SM_DirtMound": (False, 7000, 9000),
          "Crystal_": (True, 0, 0), "SM_Pillar": (True, 0, 0), "SM_Column": (True, 0, 0), "SM_Tank_": (True, 0, 0),
          "SM_tank_tracks": (False, 7000, 9000), "SM_Orc": (True, 0, 0), "SM_Tent": (True, 0, 0), "SM_Flag": (False, 9000, 11000),
          "SM_Cliffside": (True, 0, 0)}
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
EAL = unreal.EditorAssetLibrary


def report():
    bad = 0
    for pool, (paths, smax) in WALK_POOLS.items():
        for p in paths:
            if not EAL.does_asset_exist(p):
                unreal.log_warning("[pcg] MISSING %s" % p); bad += 1; continue
            m = EAL.load_asset(p); bb = m.get_bounding_box(); h = bb.max.z - bb.min.z
            ok = h * smax <= CAP_WALK_UU
            unreal.log("[pcg] %s %-70s h=%.1f uu %s" % (pool, p, h, "OK" if ok else "OVER CAP -> lower scale max to %.2f" % (CAP_WALK_UU / max(h, 1))))
            bad += 0 if ok else 1
    unreal.log("[pcg] report done, %d issues" % bad)


def anchors():
    path = "%s\\pcg_anchors_%s.csv" % (IMPORT_DIR, MAP_KEY)
    for a in eas.get_all_level_actors():
        if "KodLandmarkAnchor" in [str(t) for t in a.tags]: eas.destroy_actor(a)
    n = 0
    for row in csv.DictReader(open(path)):
        act = eas.spawn_actor_from_class(unreal.Note, unreal.Vector(float(row["X"]), float(row["Y"]), float(row["Z"])))
        act.set_actor_label("LMA_" + row["Name"])
        act.tags = [unreal.Name("KodLandmarkAnchor"), unreal.Name("LMKIND_" + row["Kind"]), unreal.Name("KodTerrainPassV2")]
        act.set_folder_path("TerrainPassV2/Anchors"); n += 1
    unreal.log("[pcg] spawned %d anchors" % n)


def volume():
    vol = None
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == "PCG_KodTerrainDress": vol = a
    if vol is None:
        vol = eas.spawn_actor_from_class(unreal.PCGVolume, unreal.Vector(0, 0, 0))
        vol.set_actor_label("PCG_KodTerrainDress"); vol.set_folder_path("TerrainPassV2")
    # PCGVolume default brush is 200 uu cube -> scale to cover map (+1000 uu z)
    vol.set_actor_scale3d(unreal.Vector(MAP_HALF_UU / 100.0, MAP_HALF_UU / 100.0, 20.0))
    vol.tags = [unreal.Name("KodTerrainPassV2")]
    comp = vol.get_component_by_class(unreal.PCGComponent)
    g = EAL.load_asset(GRAPH) if EAL.does_asset_exist(GRAPH) else None
    if g is None:
        unreal.log_warning("[pcg] graph %s not found: build it per PCG_SPEC.md, then re-run MODE=volume" % GRAPH)
    else:
        comp.set_graph(g)                                            # VERIFY: set_graph accepts UPCGGraphInterface (graph or instance)
    comp.set_editor_property("seed", SEED)
    comp.set_editor_property("generation_trigger", unreal.PCGComponentGenerationTrigger.GENERATE_ON_DEMAND)   # VERIFY enum name
    unreal.log("[pcg] volume ready seed=%d trigger=OnDemand. Click Generate on the PCG component." % SEED)


def finalize():
    n = 0
    for a in eas.get_all_level_actors():
        if a.get_actor_label() != "PCG_KodTerrainDress" and "PCG" not in a.get_class().get_name():
            continue
        for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
            c.set_mobility(unreal.ComponentMobility.STATIC)
            c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            c.set_editor_property("can_ever_affect_navigation", False)
            mesh = c.get_editor_property("static_mesh"); name = mesh.get_name() if mesh else ""
            shadow, cs, ce = next((v for k, v in RENDER.items() if k in name), (False, 7000, 9000))
            c.set_editor_property("cast_shadow", shadow)
            c.set_editor_property("instance_start_cull_distance", cs)
            c.set_editor_property("instance_end_cull_distance", ce)
            n += c.get_instance_count()
    unreal.log("[pcg] finalized %d instances (static, no collision/nav, shadows+cull per pool)" % n)


def rollback():
    k = 0
    for a in eas.get_all_level_actors():
        if "KodTerrainPassV2" in [str(t) for t in a.tags]:
            eas.destroy_actor(a); k += 1
    unreal.log("[pcg] rollback removed %d actors (volume + anchors). Save level to commit." % k)


{"report": report, "anchors": anchors, "volume": volume, "finalize": finalize, "rollback": rollback}[MODE]()
