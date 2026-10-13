# Kingdom of Dust — main menu map (UE 5.8 editor Python, headless).
# UnrealEditor-Cmd.exe KingdomOfDust.uproject -run=pythonscript -script="Tools/kod_main_menu_map.py"
#
# Creates /Game/Maps/Frontend/L_MainMenu with a PlayerStart and
# AKodFrontEndGameMode. Does not commit the .umap.
# GameDefaultMap points here. EditorStartupMap stays L_Slice0.
import unreal

PATH = "/Game/Maps/Frontend/L_MainMenu"
FOLDER = "/Game/Maps/Frontend"

def log(message):
    unreal.log("[KodMenu] " + message)

eal = unreal.EditorAssetLibrary
if not eal.does_directory_exist(FOLDER):
    eal.make_directory(FOLDER)

level_lib = unreal.EditorLevelLibrary
if eal.does_asset_exist(PATH):
    level_lib.load_level(PATH)
    log("loaded " + PATH)
else:
    level_lib.new_level(PATH)
    log("created " + PATH)

world = level_lib.get_editor_world()
settings = world.get_world_settings()
game_mode = unreal.load_class(None, "/Script/KodUI.KodFrontEndGameMode")
settings.set_editor_property("default_game_mode", game_mode)
has_start = False
for actor in level_lib.get_all_level_actors():
    if isinstance(actor, unreal.PlayerStart):
        has_start = True
        break
if not has_start:
    level_lib.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 100.0), unreal.Rotator(0.0, 0.0, 0.0))
level_lib.save_current_level()
log("saved " + PATH + " game mode KodFrontEndGameMode")
