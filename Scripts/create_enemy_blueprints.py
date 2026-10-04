"""Run using UnrealEditor-Cmd -run=pythonscript -script=<this file>.

Only creates our two new assets. Existing assets are validated, never overwritten.
"""
import unreal

tools = unreal.AssetToolsHelpers.get_asset_tools()
created = set()
for name, parent_path in (
    ("BP_WEVORAEnemy", "/Script/WEVORA.WEVORAEnemy"),
    ("BP_WEVORAEnemyProjectile", "/Script/WEVORA.WEVORAEnemyProjectile"),
):
    path = "/Game/WEVORA/AI/" + name
    parent = unreal.load_class(None, parent_path)
    if not parent:
        raise RuntimeError("Missing native class: " + parent_path)
    asset = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
    if not asset:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        asset = tools.create_asset(name, "/Game/WEVORA/AI", unreal.Blueprint, factory)
        if not asset:
            raise RuntimeError("Could not create " + path)
        created.add(name)
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    generated = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not generated or not isinstance(unreal.get_default_object(generated), getattr(unreal, parent_path.split(".")[-1])):
        raise RuntimeError("Unexpected Blueprint parent: " + path)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError("Could not save " + path)
    unreal.log("WEVORA_ENEMY_ASSET_OK " + path)

enemy_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/WEVORA/AI/BP_WEVORAEnemy")
shot_class = unreal.EditorAssetLibrary.load_blueprint_class("/Game/WEVORA/AI/BP_WEVORAEnemyProjectile")
defaults = unreal.get_default_object(enemy_class)
if "BP_WEVORAEnemy" in created:
    defaults.set_editor_property("projectile_class", shot_class)
    if not unreal.EditorAssetLibrary.save_asset("/Game/WEVORA/AI/BP_WEVORAEnemy"):
        raise RuntimeError("Could not save enemy projectile setting")
unreal.log("WEVORA_ENEMY_ASSETS_COMPLETE")
