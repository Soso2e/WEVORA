# WEVORA desktop baseline / 16GB Editor

16GB is a target, **not a verified minimum requirement**. This pass disables hardware ray tracing and Path Tracing for the product, while leaving the combat loop's C++ and binary assets unchanged. No UE installation exists in this workspace (`UE_ROOT=/workspace/UnrealEngine-5.8` is absent); the user explicitly approved skipping the build. UE automation, asset loads, shader compilation, frame rate and memory measurements remain unverified.

## Applied changes

- `r.RayTracing`, `r.Lumen.HardwareRayTracing`, `r.PathTracing`, ray tracing proxies, and hardware-RT translucent refraction: **False**. Removed inert hardware lighting-mode, RT shadow and texture-LOD overrides. Removed duplicate Windows default RHI entry.
- ModelingToolsEditorMode: explicitly disabled. There is no project source or script usage. This removes the optional modeling editor mode, not runtime gameplay. Developers needing mesh authoring can re-enable it locally.
- Android File Server runtime service and network connection: disabled for the current Desktop target. Removed its unused template security token. The plugin descriptor itself is retained pending engine plugin dependency inspection; no Android feature modules were removed.
- Project name: WEVORA. SimpleMap points to the existing `/Game/ThirdPerson/Lvl_ThirdPerson`; Content Browser defaults to `/Game/WEVORA`. GameDefaultMap, EditorStartupMap, GameMode, template class redirects and core assets are preserved.
- Added a Mac Editor launch profile, read-only UE asset audit and reproducible repository metrics script.

The RT CVar spellings follow the existing 5.8-generated project configuration and requested baseline. Their exact registration/flags in UE 5.8 could not be checked against engine source here. Check them before merge using the renderer settings, engine source and `DumpConsoleCommands` log. No numerical shader-permutation savings are claimed.

## Changes awaiting real asset evidence

| Feature | Decision / evidence needed |
| --- | --- |
| Variant_Combat / Platforming / SideScrolling, associated source and World Partition external packages | **Retained**. No Asset Registry/Referencer execution possible. Do not delete based on filenames, strings or size alone. |
| StateTree / GameplayStateTree modules and plugins, Variant include paths | **Retained** because compiled template C++ still uses StateTree. Remove only after the proven-safe Variant removal and a fresh core-reference check. |
| Substrate | **Retained**. Binary Material graphs and shader/load validation are unavailable. Front Material inputs and material functions need engine inspection; require rendered shader compilation before switching. |
| Heterogeneous Volumes, local fog, Water passes, RVT outputs, mesh-paint VT | **Retained** pending graph, actor and World Partition audits. Feature name inventory is only a lead; absence of matching filenames is not evidence. |
| VR/XR, mobile rendering | Existing VR booleans are already False. Mobile-specific settings retained rather than guessing at shared shader support without engine inspection. No platform added. |
| Niagara / EnhancedInput / AIModule / Engine / InputCore | **Retained**, used by the current spell effects, player input and gameplay. |
| UMG / Slate | **Retained**: WEVORAPlayerController uses UUserWidget, CreateWidget and SVirtualJoystick. |
| Lumen GI/reflections, mesh distance fields, Nanite, VSM, Virtual Textures, SkyAtmosphere | **Retained** product capabilities required by the brief. Skin-cache support also retained for skeletal meshes. |

## Editor profile on a 16GB Mac

```sh
python3 Scripts/editor_low_memory.py --ue-root '/Users/Shared/Epic Games/UE_5.8'
# Lower quality if Medium still exceeds available memory:
python3 Scripts/editor_low_memory.py --ue-root '/Users/Shared/Epic Games/UE_5.8' --quality low
```

Pass the installed engine directory, not its app bundle. The profile launches the actual Mac Editor process, applies Medium (or Low) scalability, 70% screen percentage and a 512 MiB texture streaming pool using process-only console commands. It never writes DefaultEngine/Scalability/platform/Shipping configuration. Texture pool size limits streamed textures only, **not total unified memory**; textures may appear blurry. Low/Medium can reduce Lumen quality in this Editor session. Nanite/VSM project support remains enabled.

For a real-time-free editing session, disable viewport Realtime (`Ctrl+R`, or the viewport menu on Mac), close unused asset editors and avoid running multiple Editors. UI-only realtime changes are documented rather than enforced with unverified startup CVars. If quality settings are saved through Editor UI, restore desired settings in that user's profile later; the launcher itself saves none.

## Reproduce UE validation once 5.8 is available

1. Build `WEVORAEditor` Development with the installed engine's `Engine/Build/BatchFiles/Build.sh` (Mac/Linux) or `Build.bat` (Windows), for the corresponding platform, with `-Project=<absolute WEVORA.uproject> -WaitMutex -NoHotReloadFromIDE`.
2. Run existing WEVORA automation on the newly built Editor:

   ```text
   UnrealEditor-Cmd <project> -unattended -nop4 -nosplash -nosound -NullRHI -ExecCmds="Automation RunTests WEVORA." -TestExit="Automation Test Queue Empty" -ReportExportPath=<absolute Saved/Automation/Optimization>
   ```

   Existing source declares 11 tests. Require all 11 to pass and inspect warnings/errors. NullRHI verifies logic; it cannot validate rendering, shaders or visual quality.
3. Run the read-only audit on a clean Editor process with PythonScriptPlugin and EditorScriptingUtilities enabled **for this invocation only**:

   ```text
   UnrealEditor-Cmd <project> -unattended -nop4 -nosplash -nosound -NullRHI -EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities -run=pythonscript -script=<absolute Scripts/audit_assets.py>
   ```

   Use the platform's command-line Editor executable (on Mac, under the installed app bundle). Audit output: `Saved/Optimization/asset-audit.json`. This script has syntax validation only in this environment, not UE API validation. Errors produce `complete=false` and raise an exception. Read the log as well: some engine warnings/errors do not raise Python exceptions.
4. Before deleting each Variant, review its hard/soft/searchable/manage external referencers, including external actors/objects and cross-Variant refs. Inspect native Blueprint parents and template class references outside Variant source, config and scripts. A group referenced by another retained group is **not safe** to delete. Engine/plugin refs and runtime string-built paths need manual review too. The script reports evidence and **never deletes assets**. Use Unreal's Referencer/Reference Viewer to corroborate, handle redirectors, then remove only proven-unreferenced packages and matching source.
5. Re-audit remaining assets before removing StateTree plugins/modules and include paths; rebuild and repeat automation and asset loads afterwards.
6. For Substrate/unused rendering features, inspect all retained project materials, material functions, instances and World Partition actors, not just the currently loaded map. The audit lists potential consumers and Front Material connections but **does not prove absence**. Disable only demonstrated-unused features; load the core map and every affected material with a real RHI, wait for shader compilation and inspect compile/load logs. Do not treat NullRHI success as shader validation.
7. Load BP_ThirdPersonCharacter, BP_WEVORAEnemy, BP_WEVORAEnemyProjectile, core spell/flight/enemy classes and `/Game/ThirdPerson/Lvl_ThirdPerson`. The audit attempts these loads without saving the map. Verify World Partition actors finish loading and there is still a persisted combat enemy.

Human verification: flight feel; attribute selection/hand effects; Fire and Wind weaving/casting; projectile visibility; enemy hit and HP reduction (100→75); environment lighting after HWRT removal. No long automated play session is required.

## Measurements

`python3 Scripts/optimization_metrics.py --base origin/main` compares tracked logical file sizes; LFS pointers count their declared payload sizes rather than pointer bytes. Git history, `.git/lfs`, DDC, Saved and Intermediate are excluded. Explicit project plugins are counted, **not all enabled-by-default engine plugins**. Deleting working-tree assets later will not immediately shrink Git/LFS history.

| Metric | main (`fafea82`) | This branch |
| --- | ---: | ---: |
| Tracked files | 882 | 886 |
| Variant files | 591 | 591 |
| Variant files removed | 0 | 0 |
| C++ .cpp / .h | 56 / 53 | 56 / 53 |
| Explicit enabled project plugins | 4 | 3 |
| Build.cs module dependencies | 11 | 11 |
| Tracked repository logical bytes | 141,355,772 | 141,379,551 |
| Content logical bytes | 140,976,421 | 140,976,421 |
| Variant logical bytes | 6,119,241 | 6,119,241 |

Content remains 134.4456 MiB. Tracked size grows slightly from scripts/documentation; no binary assets were removed. Measurements include this report. Physical workspace at the start: Content approximately 140 MB allocated, `.git` approximately 142 MB (allocation/historical storage, not directly comparable to logical byte counts).

Editor memory/startup/build/shader times: **not measured** (UE unavailable). Hardware RT/Path Tracing shader support is disabled by configuration, but shader/DDC counts and memory savings need fresh UE measurements. No 16GB operation guarantee is made.

On the same Mac, compare main and this branch with identical engine, map, viewport size, loaded World Partition cells and other running apps. Separate cold DDC/shader compilation from warmed startup. Measure after shaders finish and the same idle interval, then during the combat loop. Record Activity Monitor memory/peak and system memory pressure/swap; use `-LLM` and `stat LLM`/`stat LLMFULL` for engine allocations. LLM alone does not capture all GPU/unified memory. Record shader jobs and startup duration from logs. Do not commit caches/logs.

Re-enabling hardware RT/Path Tracing, Modeling Tools, Android support or disabling Substrate/volumes should be an explicit product decision followed by reference/shader/build checks. The largest remaining structural opportunity is Variant removal; it is deliberately awaiting engine evidence rather than claimed as complete.
