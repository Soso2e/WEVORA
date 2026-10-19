"""Set the dedicated Playground player's shape data; never edit the original player.

One-time Stage C setup. Refuses to replace an already configured delivery map.
"""
import unreal

path = '/Game/WEVORA/Playground/BP_PlaygroundPlayer'
cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
if not cls:
    raise RuntimeError('Create Playground assets first')
player = unreal.get_default_object(cls)
cast = player.get_editor_property('spell_cast_component')
if cast.get_editor_property('delivery_classes'):
    raise RuntimeError('Delivery classes already configured; preserve designer edits')
profiles = cast.get_editor_property('shape_profiles')
for gesture, delivery, values in (
    (unreal.WEVORAGesture.THRUST, unreal.WEVORASpellDeliveryKind.PROJECTILE,
     {'speed_multiplier': 1.2, 'radius_multiplier': 1.0,
      'woven_speed_multiplier': 1.4, 'woven_power_multiplier': 1.25}),
    (unreal.WEVORAGesture.SWEEP, unreal.WEVORASpellDeliveryKind.SWEEP,
     {'range': 1000.0, 'width': 650.0, 'height': 200.0, 'power_multiplier': 0.9}),
    (unreal.WEVORAGesture.CIRCLE, unreal.WEVORASpellDeliveryKind.FIELD,
     {'range': 1800.0, 'width': 380.0, 'duration': 2.4, 'pulse_interval': 0.6, 'power_multiplier': 0.35}),
    (unreal.WEVORAGesture.SLAM, unreal.WEVORASpellDeliveryKind.AREA,
     {'range': 450.0, 'width': 420.0, 'power_multiplier': 1.5}),
):
    profile = profiles[gesture]
    profile.set_editor_property('delivery', delivery)
    for key, value in values.items():
        profile.set_editor_property(key, value)
    profiles[gesture] = profile
cast.set_editor_property('shape_profiles', profiles)
cast.set_editor_property('delivery_classes', {
    unreal.WEVORASpellDeliveryKind.SWEEP: unreal.WEVORASpellVolume,
    unreal.WEVORASpellDeliveryKind.FIELD: unreal.WEVORASpellVolume,
    unreal.WEVORASpellDeliveryKind.AREA: unreal.WEVORASpellVolume,
})
cast.set_editor_property('log_events', False)
player.get_editor_property('spell_weaving_component').set_editor_property('log_events', False)
asset = unreal.load_asset(path)
unreal.BlueprintEditorLibrary.compile_blueprint(asset)
if not unreal.EditorAssetLibrary.save_loaded_asset(asset):
    raise RuntimeError('Playground spell configuration save failed')
unreal.log('PLAYGROUND_SHAPES_CONFIGURED ' + str(cast.get_editor_property('shape_profiles')))
