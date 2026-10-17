"""Create new editable prototype profiles; never overwrite existing assets.

Run once with UE Python commandlet after compiling the localized C++ classes.
The runtime sample hard-references these assets so they are included in Cook.
"""
import unreal

ROOT = "/Game/WEVORA/Localized"
SPECS = [
    ("DA_PrototypeZones", unreal.WEVORAZoneProfile),
    ("DA_FireWindReactions", unreal.WEVORALocalReactionProfile),
]
for name, _ in SPECS:
    if unreal.EditorAssetLibrary.does_asset_exist(f"{ROOT}/{name}"):
        raise RuntimeError(f"Refusing to overwrite {ROOT}/{name}")

tools = unreal.AssetToolsHelpers.get_asset_tools()
for name, asset_class in SPECS:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    asset = tools.create_asset(name, ROOT, asset_class, factory)
    if not asset or not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError(f"Could not create/save {ROOT}/{name}")
    unreal.log(f"Localized profile created: {asset.get_path_name()}")

zones = unreal.load_asset(f"{ROOT}/DA_PrototypeZones")
reactions = unreal.load_asset(f"{ROOT}/DA_FireWindReactions")
assert len(zones.get_editor_property("zones")) == 8
assert len(reactions.get_editor_property("rules")) == 1
unreal.log("LOCALIZED_PROFILES_OK: 8 zones, 1 Fire/Wind rule")
