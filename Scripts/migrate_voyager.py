import unreal

SOURCE_PACKAGES = ["/Game/Voyager/Demo/Levels/LevelCover"]
DESTINATION_CONTENT = r"C:\Users\Administrator\Documents\ChatGPT\Delta force aimtracker\Content"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
options = unreal.AssetMigrationOptions()
options.ignore_dependencies = False
options.replace_existing = False
options.skip_read_only = False

unreal.log("Migrating Voyager LevelCover and all referenced dependencies.")
success = asset_tools.migrate_packages(SOURCE_PACKAGES, DESTINATION_CONTENT, options)
if not success:
    raise RuntimeError("Voyager asset migration did not complete.")

unreal.log("Voyager migration completed successfully.")
