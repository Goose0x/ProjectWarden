# Kingdom of Dust — music, effects, and voice buses (UE 5.8 editor Python, headless).
# UnrealEditor-Cmd.exe KingdomOfDust.uproject -run=pythonscript -script="Tools/kod_audio_classes.py"
#
# Creates /Game/Audio/Classes/KodMusic, KodSFX, and KodVoice, parented to the
# project master sound class. Does not commit a .uasset. UKodUserSettings soft-loads
# these paths and skips the volume mix when they are missing or audio is off,
# so a headless soak does not push transient classes.
import unreal

PATH = "/Game/Audio/Classes"
NAMES = ("KodMusic", "KodSFX", "KodVoice")


def master_class():
    candidates = (
        "/Engine/EngineSounds/Master.Master",
        "/Engine/EngineSounds/Master",
    )
    for asset_path in candidates:
        loaded = unreal.EditorAssetLibrary.load_asset(asset_path)
        if loaded:
            return loaded
    try:
        settings = unreal.AudioSettings.get_default_object()
        soft = settings.get_editor_property("default_sound_class")
        loaded = soft.load_synchronous() if soft else None
        if loaded:
            return loaded
    except Exception as exc:
        unreal.log_warning(f"KodAudio master lookup skipped: {exc}")
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
