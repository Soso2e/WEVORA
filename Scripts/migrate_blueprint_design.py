"""One-time UE 5.8 migration. Never replaces existing design graphs.

Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>
-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities,EditorToolset.
Backs up the current on-disk player asset before saving; preserves other graphs/defaults.
"""
import json
import shutil
from datetime import datetime
from pathlib import Path
import unreal
from editor_toolset.toolsets.blueprint import BlueprintTools as BP

GRAPHS = {
    'CalculateCameraFOV': '''(fn CalculateCameraFOV (DeltaSeconds)
      (bind camera (Variables|Components|GetFollowCamera))
      (bind speed (Math|Vector|VectorLength (Transformation|GetVelocity)))
      (bind reference (Math|Float|Max(Float) (Variables|WEVORAMovement|Burst|GetBurstMaxSpeed) 1.0))
      (bind ratio (Math|Float|Clamp(Float) (/ speed reference) 0.0 1.0))
      (bind target (+ (Variables|WEVORAMovement|Camera|GetBaseCameraFOV)
                     (* (Variables|WEVORAMovement|Camera|GetSpeedFOVBoost) ratio)))
      (return (Math|Interpolation|FInterpTo
        (Class|CameraComponent|GetFieldOfView :self camera) target DeltaSeconds
        (Variables|WEVORAMovement|Camera|GetCameraFOVInterpSpeed))))''',
    'GetSpellMovementTarget': '''(fn GetSpellMovementTarget ()
      (bind weaving (Variables|Spell|GetSpellWeavingComponent))
      (switch Utilities|FlowControl|Switch|SwitchonEWEVORAWeavingState
        (Class|WEVORASpellWeavingComponent|GetState :self weaving)
        (:Weaving (return (Variables|WEVORAMovement|Spell|GetWeavingMovementMultiplier)))
        (:Shaping (return (Variables|WEVORAMovement|Spell|GetShapingMovementMultiplier)))
        (:ReadyToCast (return (Variables|WEVORAMovement|Spell|GetReadyMovementMultiplier)))
        (:Idle (return 1.0))))''',
    'GetSpellRecoilSpeed': '''(fn GetSpellRecoilSpeed (Context)
      (bind (element gesture direction magnitude duration) (Utilities|Struct|BreakWEVORASpellContext Context))
      (bind caster (Variables|Spell|GetSpellCastComponent))
      (switch Utilities|FlowControl|Switch|SwitchonEWEVORAGesture gesture
        (:Thrust (return (Class|WEVORASpellCastComponent|GetThrustRecoilSpeed :self caster)))
        (:None (return (Class|WEVORASpellCastComponent|GetRecoilSpeed :self caster)))
        (:Sweep (return (Class|WEVORASpellCastComponent|GetRecoilSpeed :self caster)))
        (:Circle (return (Class|WEVORASpellCastComponent|GetRecoilSpeed :self caster)))
        (:Slam (return (Class|WEVORASpellCastComponent|GetRecoilSpeed :self caster)))))''',
    'ResolveSpellLaunch': '''(fn ResolveSpellLaunch (Context)
      (bind caster (Variables|Spell|GetSpellCastComponent))
      (bind (element gesture direction magnitude duration) (Utilities|Struct|BreakWEVORASpellContext Context))
      (bind (profile hasElement) (Utilities|Map|Find
        (Class|WEVORASpellCastComponent|GetElementParameters :self caster) element))
      (bind (shapeProfile hasShape) (Utilities|Map|Find
        (Class|WEVORASpellCastComponent|GetShapeProfiles :self caster) gesture))
      (bind power (Class|WEVORASpellCastComponent|GetSpellPower :self caster))
      (if (and hasElement (and hasShape (> power 0.0)))
        (bind (shape delivery speedMultiplier radiusMultiplier) (Utilities|Struct|BreakWEVORASpellShapeProfile shapeProfile))
        (bind (speed radius lifetime color) (Utilities|Struct|BreakWEVORASpellProjectileParameters profile))
        (bind spell (Utilities|Struct|MakeWEVORASpellData :Element element :Shape shape :Delivery delivery :Power power))
        (bind parameters (Utilities|Struct|MakeWEVORASpellProjectileParameters
          :Speed (Math|Float|Max(Float) 1.0 (* speed speedMultiplier))
          :Radius (Math|Float|Max(Float) 1.0 (* radius radiusMultiplier))
          :Lifetime (Math|Float|Max(Float) 0.1 lifetime) :Color color))
        (bind launch (Utilities|Struct|MakeWEVORASpellLaunch :Composition Context :Spell spell :Parameters parameters))
        (return launch true)
        (else (return (Utilities|Struct|MakeWEVORASpellLaunch) false))))'''
}

asset_path = '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter'
asset = unreal.load_asset(asset_path)
if not asset:
    raise RuntimeError('Missing player Blueprint')
existing = {g.get_name() for g in BP.list_graphs(asset)}
conflicts = existing.intersection(GRAPHS)
if conflicts:
    raise RuntimeError('Design graphs already exist; edit them in the Editor. No changes saved: ' + ', '.join(sorted(conflicts)))
root = Path(unreal.Paths.project_dir())
source = root / 'Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.uasset'
backup = root / 'Saved/BlueprintDesignBackup' / datetime.now().strftime('%Y%m%d_%H%M%S')
backup.mkdir(parents=True, exist_ok=False)
shutil.copy2(source, backup / source.name)
event_before = BP.read_graph_dsl(BP.get_graph(asset, 'EventGraph'))
for name, code in GRAPHS.items():
    graph = BP.add_function_graph(asset, name)
    # The inherited function signature owns input/output pins.
    unreal.log('WEVORA_DESIGN_WRITING ' + name)
    BP.write_graph_dsl(graph, code)
BP.compile_blueprint(asset, warnings_as_errors=True)
if BP.read_graph_dsl(BP.get_graph(asset, 'EventGraph')) != event_before:
    raise RuntimeError('Existing EventGraph changed; asset not saved')
if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
    raise RuntimeError('Could not save player Blueprint')
report = {name: BP.read_graph_dsl(BP.get_graph(asset, name)) for name in GRAPHS}
(root / 'Saved/BlueprintDesignMigration.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('WEVORA_BLUEPRINT_DESIGN_MIGRATION_OK backup=' + str(backup))
