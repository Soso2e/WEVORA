"""Create only new Playground assets. Refuse overwrites; leave existing maps/BPs untouched.

UnrealEditor-Cmd WEVORA.uproject -run=pythonscript -script=<this script>
-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -unattended -NullRHI
"""
import json
from pathlib import Path
import unreal

ROOT = '/Game/WEVORA/Playground'
MAP = '/Game/WEVORA/Maps/L_CombatPlayground'
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
paths = [ROOT + '/BP_PlaygroundPlayer', ROOT + '/BP_PlaygroundGameMode',
         ROOT + '/BP_PlaygroundTarget', MAP]
if any(assets.does_asset_exist(p) for p in paths):
    raise RuntimeError('Playground assets already exist; inspect and edit them instead of overwriting.')

def duplicate(source, destination):
    result = assets.duplicate_asset(source, destination)
    if not result:
        raise RuntimeError('Duplicate failed: ' + destination)
    return result

player = duplicate('/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter', paths[0])
mode = duplicate('/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode', paths[1])
factory = unreal.BlueprintFactory()
factory.set_editor_property('parent_class', unreal.WEVORAPlaygroundTarget)
target = tools.create_asset('BP_PlaygroundTarget', ROOT, unreal.Blueprint, factory)
for asset in (player, mode, target):
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
player_cls = assets.load_blueprint_class(paths[0])
mode_cls = assets.load_blueprint_class(paths[1])
target_cls = assets.load_blueprint_class(paths[2])
unreal.get_default_object(mode_cls).set_editor_property('default_pawn_class', player_cls)
# Reuse the original enemy's saved reaction and projectile data, not guessed C++ values.
enemy_cls = assets.load_blueprint_class('/Game/WEVORA/AI/BP_WEVORAEnemy')
enemy_defaults = unreal.get_default_object(enemy_cls)
target_defaults = unreal.get_default_object(target_cls)
target_defaults.get_editor_property('spell_reaction_component').set_editor_property(
    'reaction_rules', enemy_defaults.get_editor_property('spell_reaction_component').get_editor_property('reaction_rules'))
target_defaults.set_editor_property('projectile_class', enemy_defaults.get_editor_property('projectile_class'))
for asset in (player, mode, target):
    unreal.BlueprintEditorLibrary.compile_blueprint(asset)
    if not assets.save_loaded_asset(asset):
        raise RuntimeError('Blueprint save failed')

if not level.new_level(MAP):
    raise RuntimeError('Independent level creation failed')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', mode_cls)
world.get_world_settings().set_editor_property('kill_z', -2000)

def spawn(cls, name, xyz, rotation=None):
    obj = actors.spawn_actor_from_class(cls, unreal.Vector(*xyz), rotation or unreal.Rotator())
    if not obj:
        raise RuntimeError('Spawn failed: ' + name)
    obj.set_actor_label(name)
    return obj

cube = unreal.load_asset('/Engine/BasicShapes/Cube')
def box(name, xyz, scale):
    obj = spawn(unreal.StaticMeshActor, name, xyz)
    obj.static_mesh_component.set_static_mesh(cube)
    obj.set_actor_scale3d(unreal.Vector(*scale))
    return obj

box('Flight field 240m x 240m', (0, 0, -75), (240, 240, 1.5))
for x in range(-10000, 10001, 2000):
    box('Distance grid X ' + str(x), (x, 0, 2), (0.035, 200, 0.03))
    box('Distance grid Y ' + str(x), (0, x, 2), (200, 0.035, 0.03))
for x, y in ((-10000,-10000),(-10000,10000),(10000,-10000),(10000,10000)):
    box('Boundary beacon', (x, y, 1500), (1, 1, 30))
# A separate, labelled occlusion lane; open practice lanes remain unobstructed.
box('Cover comparison wall', (2600, -2400, 350), (1, 8, 7))
spawn(unreal.PlayerStart, 'Playground Player Start', (0, 0, 120))
spawn(unreal.WEVORACombatPlayground, 'Combat Playground - tuning and reset', (0, 0, 0))
sun = spawn(unreal.DirectionalLight, 'Sun', (0, 0, 4000), unreal.Rotator(-50, -30, 0))
sun.light_component.set_editor_property('intensity', 4.0)
sky = spawn(unreal.SkyLight, 'Sky fill', (0, 0, 3000))
sky.light_component.set_editor_property('intensity', 1.5)
sky.light_component.set_editor_property('real_time_capture', True)
spawn(unreal.SkyAtmosphere, 'Sky', (0, 0, 0))

targets = [
    ('01 Stationary / Fire burn', (1600, -600, 280), (0,1,0), 0, 0, False),
    ('02 Strafe / lead your shot', (2400, 0, 550), (0,1,0), 650, 500, False),
    ('03 Vertical / airborne aim', (3200, 800, 1000), (0,0,1), 600, 420, False),
    ('04 Ranged opponent', (5000, 1600, 700), (0,1,0), 0, 0, True),
    ('05 Cover comparison', (3400, -2400, 350), (0,1,0), 250, 250, False),
]
for name, xyz, axis, amplitude, speed, combat in targets:
    obj = spawn(target_cls, name, xyz)
    for key, value in {'target_name': name, 'motion_axis': unreal.Vector(*axis),
                       'motion_amplitude': amplitude, 'motion_speed': speed, 'combat_ai': combat,
                       'detection_range': 3600 if combat else 0, 'attack_range': 2300,
                       'attack_interval': 2.0, 'windup_time': 0.65}.items():
        obj.set_editor_property(key, value)
    obj.get_editor_property('health_component').set_editor_property('max_health', 150)
    # Large readable targets; collision radius and body agree.
    obj.get_editor_property('collision').set_sphere_radius(65)
    obj.get_editor_property('body').set_relative_scale3d(unreal.Vector(1.3, 1.3, 1.3))

if not level.save_current_level():
    raise RuntimeError('Playground map save failed')
if not level.load_level(MAP):
    raise RuntimeError('Playground persisted map reload failed')
saved = actors.get_all_level_actors()
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
report = {'map': MAP, 'targets': [a.get_actor_label() for a in saved if isinstance(a, unreal.WEVORAPlaygroundTarget)],
          'actor_count': len(saved), 'game_mode': world.get_world_settings().get_editor_property('default_game_mode').get_path_name()}
if len(report['targets']) != 5:
    raise RuntimeError('Expected five persisted targets')
out = Path(unreal.Paths.project_saved_dir()) / 'CombatPlayground/MapCreation.json'
out.write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('PLAYGROUND_MAP_CREATED ' + json.dumps(report))
