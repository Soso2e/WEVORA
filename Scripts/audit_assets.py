"""Read-only UE 5.8 audit. Run with PythonScriptPlugin/EditorScriptingUtilities.

Never deletes or saves assets. Writes a unique Saved/Optimization/asset-audit-*.json.
Any error makes the report incomplete; a report is not deletion authorization.
"""
import json
from datetime import datetime, timezone
from pathlib import Path
import re
import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
VARIANTS = ('Variant_Combat', 'Variant_Platforming', 'Variant_SideScrolling')
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(synchronous_search=True)
registry.wait_for_completion()
# Include soft references, searchable names and management dependencies too.
options = unreal.AssetRegistryDependencyOptions(
    include_soft_package_references=True, include_hard_package_references=True,
    include_searchable_names=True, include_soft_management_references=True,
    include_hard_management_references=True)
assets = registry.get_assets_by_path('/Game', recursive=True, include_only_on_disk_assets=True)
report = {
    'engine_version': unreal.SystemLibrary.get_engine_version(),
    'complete': False, 'errors': [], 'variants': {}, 'feature_candidates': [],
    'substrate_front_inputs': [], 'required_loads': {},
    'limitations': [
        'Registry edges do not prove absence of dynamically constructed asset paths.',
        'World Partition external actor packages are scanned from disk explicitly.',
        'Feature class/name candidates require material and map inspection before disabling.',
        'Substrate front inputs alone do not prove that legacy conversion is safe.',
        'Blueprint parent tags and source class names need manual review before source removal.'
    ]}
if not report['engine_version'].startswith('5.8.'):
    report['errors'].append('This audit requires the project\'s UE 5.8 engine.')
if not assets:
    report['errors'].append('Asset Registry returned no /Game assets; inventory is incomplete.')


def package_for(path):
    return '/Game/' + path.relative_to(ROOT / 'Content').with_suffix('').as_posix()


def belongs(package, variant):
    return any(package.startswith(prefix + variant + '/') for prefix in (
        '/Game/', '/Game/__ExternalActors__/', '/Game/__ExternalObjects__/'))


def dependency_closure(roots):
    visited = set()
    pending = list(roots)
    while pending:
        package = pending.pop()
        if package in visited:
            continue
        visited.add(package)
        pending.extend(str(dep) for dep in registry.get_dependencies(package, options)
                       if str(dep) not in visited)
    return sorted(visited)


def native_types(headers):
    # Template UObject classes can omit WEVORA_API. Match definitions with a
    # base type, not forward declarations, so their Blueprint children are audited.
    source = '\n'.join(p.read_text(encoding='utf-8') for p in headers.rglob('*.h'))
    return sorted(set(re.findall(
        r'\b(?:class|struct)\s+(?:WEVORA_API\s+)?(\w+)\s*:', source)))


for variant in VARIANTS:
    packages = set()
    for folder in (ROOT / 'Content' / variant,
                   ROOT / 'Content/__ExternalActors__' / variant,
                   ROOT / 'Content/__ExternalObjects__' / variant):
        if folder.exists():
            packages.update(package_for(p) for p in folder.rglob('*')
                            if p.suffix in ('.uasset', '.umap'))
    # Force scanning external packages rather than relying on Content Browser visibility.
    try:
        registry.scan_files_synchronous([
            str(ROOT / 'Content' / (p[len('/Game/'):])) +
            ('.umap' if (ROOT / 'Content' / (p[len('/Game/'):])).with_suffix('.umap').exists()
             else '.uasset') for p in sorted(packages)], force_rescan=True)
        # Refresh after scanning: parent tags from the initial inventory may be stale.
        assets = registry.get_assets_by_path('/Game', recursive=True, include_only_on_disk_assets=True)
        external = {}
        for package in sorted(packages):
            refs = [str(ref) for ref in registry.get_referencers(package, options)
                    if not belongs(str(ref), variant)]
            if refs:
                external[package] = sorted(refs)
        headers = ROOT / 'Source/WEVORA' / variant
        classes = native_types(headers)
        blueprint_parents = []
        for data in assets:
            if belongs(str(data.package_name), variant):
                continue
            for tag in ('ParentClass', 'NativeParentClass'):
                value = str(data.get_tag_value(tag) or '')
                if any(re.search(r'\b' + re.escape(c[1:]) + r'\b', value) for c in classes):
                    blueprint_parents.append({'asset': str(data.package_name), 'tag': tag, 'value': value})
        report['variants'][variant] = {
            'package_count': len(packages), 'packages': sorted(packages),
            'external_referencers': external,
            'native_classes': classes, 'outside_blueprint_parents': blueprint_parents,
            'deletion_status': 'manual review required; no automatic deletion'}
    except Exception as error:
        report['errors'].append(f'{variant}: {error}')

# Inventory possible feature consumers; names are audit leads, never unused evidence.
for data in assets:
    class_name = str(data.asset_class_path.asset_name)
    path = str(data.package_name)
    if any(word.lower() in (class_name + ' ' + path).lower() for word in
           ('Volume', 'Water', 'VirtualTexture', 'Fog', 'StateTree', 'Substrate')):
        report['feature_candidates'].append({'asset': path, 'class': class_name})
    if class_name == 'Material':
        try:
            material = data.get_asset()
            if material is None:
                raise RuntimeError('Material load returned None')
            prop = unreal.MaterialProperty.MP_FRONT_MATERIAL
            node = unreal.MaterialEditingLibrary.get_material_property_input_node(material, prop)
            if node:
                report['substrate_front_inputs'].append({
                    'asset': path, 'node_class': node.get_class().get_name()})
        except Exception as error:
            report['errors'].append(f'Material {path}: {error}')

required = (
    '/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter',
    '/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode',
    '/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController',
    '/Game/WEVORA/AI/BP_WEVORAEnemy',
    '/Game/WEVORA/AI/BP_WEVORAEnemyProjectile',
)
try:
    # Include the core map's externally packaged actors/objects even when World
    # Partition cells are not loaded. Scan from disk, not just Content Browser.
    core_external_files = [p for folder in (
        ROOT / 'Content/__ExternalActors__/ThirdPerson',
        ROOT / 'Content/__ExternalObjects__/ThirdPerson')
        if folder.exists() for p in folder.rglob('*.uasset')]
    registry.scan_files_synchronous([str(p) for p in core_external_files], force_rescan=True)
    roots = ['/Game/ThirdPerson/Lvl_ThirdPerson', *required,
             *(package_for(p) for p in core_external_files)]
    closure = dependency_closure(roots)
    report['core_dependency_roots'] = sorted(set(roots))
    report['core_dependency_closure'] = closure
    report['core_variant_dependencies'] = [p for p in closure if any(
        belongs(p, variant) for variant in VARIANTS)]
except Exception as error:
    report['errors'].append(f'Core dependency closure: {error}')

for path in required:
    try:
        loaded = unreal.EditorAssetLibrary.load_blueprint_class(path)
        report['required_loads'][path] = bool(loaded)
        if not loaded:
            raise RuntimeError('Blueprint class load returned None')
    except Exception as error:
        report['errors'].append(f'{path}: {error}')
for name in ('WEVORACharacter', 'WEVORAEnemy', 'WEVORAEnemyProjectile',
             'WEVORASpellWeavingComponent', 'WEVORASpellCastComponent',
             'WEVORASpellSelectionEffectComponent', 'WEVORASpellProjectile',
             'WEVORAFlightMovementComponent', 'WEVORAManaComponent', 'WEVORAHealthComponent'):
    path = '/Script/WEVORA.' + name
    try:
        loaded = unreal.load_class(None, path)
        report['required_loads'][path] = bool(loaded)
        if not loaded:
            raise RuntimeError('Native class load returned None')
    except Exception as error:
        report['errors'].append(f'{path}: {error}')
try:
    map_path = '/Game/ThirdPerson/Lvl_ThirdPerson'
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    report['required_loads'][map_path] = level.load_level(map_path)
    if not report['required_loads'][map_path]:
        raise RuntimeError('Core map load failed')
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    report['core_map_actor_classes'] = sorted(set(actor.get_class().get_path_name() for actor in actors))
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    if not world:
        raise RuntimeError('No Editor world after core map load')
    report['console_queries'] = [
        'r.RayTracing', 'r.Lumen.HardwareRayTracing', 'r.PathTracing',
        'r.RayTracing.RayTracingProxies.ProjectEnabled',
        'r.Lumen.Reflections.HardwareRayTracing.Translucent.Refraction.EnableForProject',
        'r.DynamicGlobalIlluminationMethod', 'r.ReflectionMethod',
        'r.GenerateMeshDistanceFields', 'r.Nanite.ProjectEnabled',
        'r.Shadow.Virtual.Enable', 'r.VirtualTextures', 'r.Substrate',
        'r.HeterogeneousVolumes',
    ]
    for name in report['console_queries']:
        # Read-only queries; inspect log for registration/value/LastSetBy.
        # Unknown console commands can log errors without throwing in Python.
        unreal.SystemLibrary.execute_console_command(world, name)
except Exception as error:
    report['errors'].append(str(error))
report['complete'] = not report['errors']
stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
output = ROOT / 'Saved/Optimization' / f'asset-audit-{stamp}.json'
output.parent.mkdir(parents=True, exist_ok=True)
output.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding='utf-8')
unreal.log('WEVORA_ASSET_AUDIT ' + str(output))
if not report['complete']:
    raise RuntimeError('Audit incomplete; see report errors. Do not delete assets.')
