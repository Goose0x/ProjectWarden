# Kingdom of Dust — music, effects, and voice buses (UE 5.8 editor Python, headless).
# UnrealEditor-Cmd.exe KingdomOfDust.uproject -run=pythonscript -script="Tools/kod_audio_classes.py"
#
# Creates /Game/Audio/Classes/KodMusic, KodSFX, and KodVoice, parented to the
# project master sound class. Does not commit a .uasset. UKodUserSettings soft-loads
# these paths and skips the volume mix when they are missing or audio is off,
# so a headless soak does not push transient classes.
import os
import unreal

PATH = "/Game/Audio/Classes"
NAMES = ("KodMusic", "KodSFX", "KodVoice")
FALLBACK_MASTER = "/Engine/EngineSounds/Master.Master"


def load_sound_class(asset_path):
    if not asset_path:
        return None
    loaded = unreal.EditorAssetLibrary.load_asset(asset_path)
    if isinstance(loaded, unreal.SoundClass):
        return loaded
    try:
        loaded = unreal.load_object(None, asset_path)
    except Exception:
        loaded = None
    if isinstance(loaded, unreal.SoundClass):
        return loaded
    return None


def default_sound_class_from_config():
    roots = []
    try:
        roots.append(unreal.Paths.project_config_dir())
    except Exception:
        pass
    try:
        roots.append(unreal.Paths.engine_config_dir())
    except Exception:
        pass
    names = ("DefaultEngine.ini", "BaseEngine.ini")
    for root in roots:
        for name in names:
            ini_path = os.path.join(root, name)
            if not os.path.isfile(ini_path):
                continue
            with open(ini_path, "r", encoding="utf-8", errors="ignore") as handle:
                for line in handle:
                    stripped = line.strip()
                    if stripped.startswith("DefaultSoundClassName="):
                        return stripped.split("=", 1)[1].strip().strip('"')
    return ""


def master_class():
    configured = default_sound_class_from_config()
    for asset_path in (configured, FALLBACK_MASTER, "/Engine/EngineSounds/Master"):
        loaded = load_sound_class(asset_path)
        if loaded:
            unreal.log(f"KodAudio master {asset_path}")
            return loaded
    unreal.log_warning("KodAudio master sound class was not found; buses will have no parent")
    return None


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    unreal.EditorAssetLibrary.make_directory(PATH)
    parent = master_class()
    if not parent:
        unreal.log_warning("KodAudio master sound class was not found; buses will have no parent")
    factory = unreal.SoundClassFactory()
    for name in NAMES:
        asset_path = f"{PATH}/{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        else:
            asset = tools.create_asset(name, PATH, unreal.SoundClass, factory)
        if not asset:
            unreal.log_error(f"KodAudio failed to create {asset_path}")
            continue
        if parent:
            asset.set_editor_property("parent_class", parent)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        unreal.log(f"KodAudio saved {asset_path}")


if __name__ == "__main__":
    main()
