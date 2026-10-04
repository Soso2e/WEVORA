"""Place one existing enemy in the existing first map. Preserve all existing actors/settings.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>, with
PythonScriptPlugin and EditorScriptingUtilities enabled for that invocation only.
"""
import unreal

MAP = "/Game/ThirdPerson/Lvl_ThirdPerson"
ENEMY = "/Game/WEVORA/AI/BP_WEVORAEnemy"
LABEL = "WEVORA_CombatEnemy"
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors_api = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not level.load_level(MAP):
    raise RuntimeError("Could not load " + MAP)
actors = actors_api.get_all_level_actors()
enemy_class = unreal.EditorAssetLibrary.load_blueprint_class(ENEMY)
if not enemy_class:
    raise RuntimeError("Missing existing enemy Blueprint")
existing = [actor for actor in actors if isinstance(actor, unreal.WEVORAEnemy)]
if existing:
    for actor in existing:
        unreal.log("COMBAT_ENEMY_EXISTING " + actor.get_actor_label() + " " + str(actor.get_actor_location()))
else:
    starts = [actor for actor in actors if isinstance(actor, unreal.PlayerStart)]
    if len(starts) != 1:
        raise RuntimeError("Expected exactly one PlayerStart; map left unchanged")
    start = starts[0]
    position = start.get_actor_location() + start.get_actor_forward_vector() * 1100.0 + unreal.Vector(0, 0, 150)
    enemy = actors_api.spawn_actor_from_class(enemy_class, position, unreal.Rotator(0, 180, 0))
    if not enemy:
        raise RuntimeError("Enemy spawn failed")
    enemy.set_actor_label(LABEL)
    health = enemy.get_editor_property("health_component")
    health.set_editor_property("show_damage_feedback", True)
    if not level.save_current_level():
        raise RuntimeError("Map save failed")
    unreal.log("COMBAT_ENEMY_CREATED " + LABEL + " " + str(enemy.get_actor_location()))

# Reload the saved map so this is persisted-asset verification, not just an in-memory spawn.
if not level.load_level(MAP):
    raise RuntimeError("Could not reload saved map")
saved_enemies = [actor for actor in actors_api.get_all_level_actors() if isinstance(actor, unreal.WEVORAEnemy)]
if not saved_enemies:
    raise RuntimeError("Saved map contains no enemy")
for actor in saved_enemies:
    health = actor.get_editor_property("health_component")
    unreal.log("COMBAT_MAP_READY " + MAP + " enemy=" + actor.get_actor_label()
               + " maxHP=" + str(health.get_editor_property("max_health"))
               + " feedback=" + str(health.get_editor_property("show_damage_feedback")))
