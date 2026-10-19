"""Create new opt-in Solo assets; never rewrite the existing practice map or BPs."""
import unreal

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
map_path = '/Game/WEVORA/Maps/L_SoloArenaPrototype'
config_path = '/Game/WEVORA/Solo/DA_SoloArenaPrototype'
if any(assets.does_asset_exist(p) for p in (map_path, config_path)):
    raise RuntimeError('Solo assets already exist. Preserve designer edits; inspect before changing.')
factory = unreal.DataAssetFactory()
factory.set_editor_property('data_asset_class', unreal.WEVORAArenaConfig)
config = tools.create_asset('DA_SoloArenaPrototype', '/Game/WEVORA/Solo', unreal.WEVORAArenaConfig, factory)
if not config or not assets.save_loaded_asset(config):
    raise RuntimeError('Config creation failed')
if not level.new_level(map_path):
    raise RuntimeError('New solo map creation failed')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.WEVORASoloGameMode)
world.get_world_settings().set_editor_property('kill_z', -2000)

def spawn(cls, label, xyz, rotation=None):
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*xyz), rotation or unreal.Rotator())
    if not actor:
        raise RuntimeError('Spawn failed: ' + label)
    actor.set_actor_label(label)
    return actor

cube = unreal.load_asset('/Engine/BasicShapes/Cube')

def box(label, xyz, scale):
    actor = spawn(unreal.StaticMeshActor, label, xyz)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector(*scale))

box('Solo arena 240m', (0, 0, -75), (240, 240, 1.5))
for pos, scale in (((-11500,0,500),(1,230,10)), ((11500,0,500),(1,230,10)),
                   ((0,-11500,500),(230,1,10)), ((0,11500,500),(230,1,10))):
    box('Arena boundary', pos, scale)
for x in range(-10000, 10001, 2000):
    box('Distance grid', (x,0,2), (0.03,200,0.03))
    box('Distance grid', (0,x,2), (200,0.03,0.03))
spawn(unreal.PlayerStart, 'Solo start', (0,0,120))
director = spawn(unreal.WEVORARunDirector, 'Solo run director', (0,0,0))
director.set_editor_property('config', config)
sun = spawn(unreal.DirectionalLight, 'Sun', (0,0,4000), unreal.Rotator(-50,-30,0))
sun.light_component.set_editor_property('intensity', 4.0)
sky = spawn(unreal.SkyLight, 'Sky fill', (0,0,3000))
sky.light_component.set_editor_property('intensity', 1.5)
sky.light_component.set_editor_property('real_time_capture', True)
spawn(unreal.SkyAtmosphere, 'Sky', (0,0,0))
if not level.save_current_level():
    raise RuntimeError('Solo level save failed')
unreal.log('SOLO_ARENA_CREATED ' + map_path)
