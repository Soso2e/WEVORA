"""Create the reusable attack material and add Lightning to existing design defaults.

Run with UnrealEditor-Cmd -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities
-run=pythonscript -script=<absolute path>. Use -EffectsMaterialOnly before building new C++.
Existing material graphs and user-tuned Fire/Wind settings are preserved.
"""
import unreal

path = "/Game/WEVORA/Effects/M_AttackEffect"
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_AttackEffect", "/Game/WEVORA/Effects", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    color = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
    color.set_editor_property("parameter_name", "SpellColor")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.15, 0.02, 1.0))
    intensity = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 180)
    intensity.set_editor_property("parameter_name", "EmissiveIntensity")
    intensity.set_editor_property("default_value", 6.0)
    multiply = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionMultiply, -150, 0)
    unreal.MaterialEditingLibrary.connect_material_expressions(color, "", multiply, "A")
    unreal.MaterialEditingLibrary.connect_material_expressions(intensity, "", multiply, "B")
    unreal.MaterialEditingLibrary.connect_material_property(multiply, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save attack material")

material = unreal.load_asset(path)
if not material.get_editor_property("used_with_instanced_static_meshes"):
    material.set_editor_property("used_with_instanced_static_meshes", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save instanced mesh usage")

if "EffectsMaterialOnly" not in unreal.SystemLibrary.get_command_line():
    shot_path = "/Game/WEVORA/Effects/BP_WEVORASpellProjectile"
    if not unreal.EditorAssetLibrary.does_asset_exist(shot_path):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.WEVORASpellProjectile.static_class())
        shot_blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "BP_WEVORASpellProjectile", "/Game/WEVORA/Effects", unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(shot_blueprint)
        if not unreal.EditorAssetLibrary.save_loaded_asset(shot_blueprint):
            raise RuntimeError("Could not save attack Blueprint")
    pilot_path = "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"
    pilot = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(pilot_path))
    weaving = pilot.get_editor_property("spell_weaving_component")
    elements = list(weaving.get_editor_property("available_elements"))
    if unreal.WEVORASpellElement.LIGHTNING not in elements:
        elements.append(unreal.WEVORASpellElement.LIGHTNING)
        weaving.set_editor_property("available_elements", elements)
    caster = pilot.get_editor_property("spell_cast_component")
    if caster.get_editor_property("projectile_class") == unreal.WEVORASpellProjectile.static_class():
        caster.set_editor_property("projectile_class", unreal.EditorAssetLibrary.load_blueprint_class(shot_path))
    profiles = dict(caster.get_editor_property("element_parameters"))
    if unreal.WEVORASpellElement.LIGHTNING not in profiles:
        lightning = unreal.WEVORASpellProjectileParameters()
        lightning.set_editor_property("speed", 3200.0)
        lightning.set_editor_property("color", unreal.LinearColor(0.65, 0.3, 1.0, 1.0))
        profiles[unreal.WEVORASpellElement.LIGHTNING] = lightning
        caster.set_editor_property("element_parameters", profiles)
    blueprint = unreal.load_asset(pilot_path)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint):
        raise RuntimeError("Could not save Lightning defaults")
unreal.log("WEVORA_ATTACK_EFFECTS_COMPLETE")
