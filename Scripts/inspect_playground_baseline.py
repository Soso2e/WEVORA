"""Read saved Blueprint defaults without saving or migrating existing assets."""
import json
from pathlib import Path
import unreal

report = {}
for name, path in {
    'player': '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter',
    'enemy': '/Game/WEVORA/AI/BP_WEVORAEnemy',
    'projectile': '/Game/WEVORA/Effects/BP_WEVORASpellProjectile',
}.items():
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not cls:
        raise RuntimeError('Missing saved Blueprint: ' + path)
    obj = unreal.get_default_object(cls)
    entry = {'class': cls.get_path_name(), 'components': {}}
    for prop in ('cruise_speed', 'burst_speed', 'fall_gravity_scale', 'glide_input_gravity_multiplier',
                 'ground_speed', 'move_speed', 'detection_range', 'attack_range'):
        try:
            entry[prop] = str(obj.get_editor_property(prop))
        except Exception:
            pass
    for comp in obj.get_components_by_class(unreal.ActorComponent):
        values = {}
        for prop in ('element_parameters', 'shape_profiles', 'delivery_classes', 'spell_power',
                     'horizontal_velocity_inheritance', 'inherit_shooter_velocity', 'spawn_distance',
                     'max_charge_power_multiplier', 'max_charge_time', 'attack_cooldown',
                     'reaction_rules', 'max_health', 'element_effects'):
            try:
                values[prop] = str(comp.get_editor_property(prop))
            except Exception:
                pass
        if values:
            entry['components'][comp.get_name()] = values
    report[name] = entry
out = Path(unreal.Paths.project_saved_dir()) / 'CombatPlayground/BlueprintDefaultsBefore.json'
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
unreal.log('PLAYGROUND_BASELINE_DEFAULTS ' + str(out))
