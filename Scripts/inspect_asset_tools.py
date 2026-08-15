import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
unreal.log("migrate_packages help: " + str(asset_tools.migrate_packages.__doc__))
unreal.log("asset tools migration names: " + str([name for name in dir(unreal) if "migrat" in name.lower()]))
