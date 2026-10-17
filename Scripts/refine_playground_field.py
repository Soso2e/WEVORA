"""Apply simple readable materials to the new map only; preserve every actor/setting."""
import unreal

MAP = '/Game/WEVORA/Maps/L_CombatPlayground'
ROOT = '/Game/WEVORA/Playground'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
materials = {}
for name, color, glow in (
    ('M_PlaygroundFloor', (0.055, 0.075, 0.10), 0),
    ('M_PlaygroundGrid', (0.12, 0.25, 0.34), 0.25),
    ('M_PlaygroundCover', (0.24, 0.14, 0.08), 0),
):
    path = ROOT + '/' + name
    material = unreal.load_asset(path) if assets.does_asset_exist(path) else None
    if not material:
        material = tools.create_asset(name, ROOT, unreal.Material, unreal.MaterialFactoryNew())
        color_node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -300, 0)
        color_node.set_editor_property('constant', unreal.LinearColor(*color, 1))
        unreal.MaterialEditingLibrary.connect_material_property(color_node, '', unreal.MaterialProperty.MP_BASE_COLOR)
        rough = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 150)
        rough.set_editor_property('r', 0.95)
        unreal.MaterialEditingLibrary.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
        if glow:
            emit = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -300, 300)
            emit.set_editor_property('constant', unreal.LinearColor(*(v * glow for v in color), 1))
            unreal.MaterialEditingLibrary.connect_material_property(emit, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(material)
        if not assets.save_loaded_asset(material):
            raise RuntimeError('Material save failed: ' + path)
    materials[name] = material
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level(MAP):
    raise RuntimeError('Map load failed')
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    label = actor.get_actor_label()
    key = 'M_PlaygroundFloor' if label.startswith('Flight field') else 'M_PlaygroundCover' if label.startswith('Cover') else 'M_PlaygroundGrid'
    actor.static_mesh_component.set_material(0, materials[key])
if not level.save_current_level():
    raise RuntimeError('Refined map save failed')
unreal.log('PLAYGROUND_FIELD_MATERIALS_SAVED')
