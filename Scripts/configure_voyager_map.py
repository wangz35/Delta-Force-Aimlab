import unreal

MAP_PATH = "/Game/Voyager/Demo/Levels/LevelCover"
GAME_MODE_PATH = "/Script/AimTracker.AimTrainerGameMode"

world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
if not world:
    raise RuntimeError("Unable to load migrated Voyager LevelCover map.")

game_mode_class = unreal.load_class(None, GAME_MODE_PATH)
if not game_mode_class:
    raise RuntimeError("Unable to load AimTrainerGameMode class.")

world_settings = world.get_world_settings()
world_settings.set_editor_property("default_game_mode", game_mode_class)

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Unable to save the configured Voyager level.")

unreal.log("LevelCover now uses AimTrainerGameMode.")
