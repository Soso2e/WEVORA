# Solo Arena Vertical Slice — 実装指示 / Technical Plan

> Status: Phase A–D implementation and validation recorded in [SoloArena.md](SoloArena.md) and [../PROGRESS.md](../PROGRESS.md). This file preserves the original implementation plan, created 2026-10-19. Human play acceptance remains outstanding.
> Working branch: `feature/solo-arena-vertical-slice`, based on `feature/localized-elemental-reactions` (draft PR #9, stacked on draft PR #8).
> Product direction: `main:Docs/SoloArenaDesign.md` (2026-10-17). That design document exists on main, not necessarily on this stacked branch. **Integrate the latest main documentation and preserve its newer decisions before implementing.**
> Do not merge #8/#9 or overwrite existing Content/Config without explicit need; do not change the original Combat Playground into a run mode.

## Player-facing goal

Build the **first complete, repeatable single-player run**: enter one large arena → fight progressively spawning waves → pick 1 of 3 run-only upgrades between waves → fight a boss with readable tells → win/lose → retry, resetting run upgrades → on first clear unlock a trial-associated element that remains available in subsequent runs.

Prioritize the already-established **fast flight + free-form weaving** mechanics. Do not gate or restrict elemental reactions behind upgrade cards.

## Scope and pacing

- Initial playable hypothesis: 5 waves, a simple boss encounter, 3 options per reward. These counts are **tuning data**, not permanent design decisions.
- Solo first. Do not build PvP, Steam lobby, matchmaking, network replication, monetization, final art, completed Niagara or audio in this PR.
- Keep `/Game/WEVORA/Maps/L_CombatPlayground` and its test targets/shortcuts operational for shooting and localized-reaction verification.
- Prefer separate `L_SoloArenaPrototype` (may reuse/duplicate existing large arena geometry as a starting point), opt-in Solo GameMode / run director and UI. No maze or multi-room world.

## Implementation order (small reviewable commits)

### A — Arena run director, wave spawning, combat progression
1. Introduce an explicit run state machine (e.g. Idle/Preparing/WaveActive/UpgradeChoice/BossFight/Victory/Defeat) with transition guards, timeouts and deterministic reset.
2. Define data-driven wave configuration (enemy classes/types, spawn points/volumes, simultaneous cap, spawn cadence, wave end rule, difficulty modifiers, boss phase). Preserve one large arena.
3. Spawn waves incrementally, not all enemies at once. Count only run-owned enemies. Track health/death events and clean delegates/timers/actors on end or reset. Never count existing Combat Playground practice targets; they should not auto-respawn in a solo run.
4. Include minimal on-screen wave/countdown/remaining enemies/player HP/mana feedback. The player can attack and be attacked throughout; no scripted victory via fake hit points.
5. On player death/bounds handling, end the run once. Ensure a simultaneous death and final kill does not advance into a reward/victory from a failed run.
6. Keep enemy counts, projectiles, VFX/volumes bounded; avoid unbounded Tick/actor creation.

### B — Three upgrade families and choice UX
1. Data-drive run-scoped choices: **Element**, **Mobility**, **Survival**. Provide at least one meaningful test upgrade from each family.
2. Examples: Element = radius or range increase (not reaction unlock), Mobility = flight speed or mana recovery improvement, Survival = instant heal or max HP increase.
3. For first iteration, show three distinct options, preferably one from each family, after non-final wave; allow keyboard and mouse selection. Disable shooting/weaving while choosing, then cleanly restore controls.
4. Implement centralized application/removal (modifier layer or initial-stat snapshots). Do NOT destructively overwrite Blueprint-authored defaults. Upgrades must clear on defeat, victory, restart and a fresh run, including modified mana/HP and spell properties. Preserve predictable stacking/caps.
5. All base reactions (including localized Fire→Wind interactions) work **without taking an upgrade**. Modifier changes to radius/power must pass through existing SpellData → Delivery → Reaction → Health.
6. Starting Solo spell access is **Fire and Wind**; do not assume Lightning is initially unlocked just because it exists in the Playground prototype. Keep the practice playground's access intact.

### C — Boss, trial reward and permanent save
1. Make one **simple boss** using the existing enemy Health/AI/projectile/reaction path, with visible windup/telegraph, attack, recovery and vulnerability/counterattack window. No new damage subsystem.
2. Introduce data-driven trial ID → element reward. Example for full end-to-end testing: **Lightning trial** using the already implemented Lightning projectile; do not hardwire progression to Lightning. **Water trial** stays an authored example/future content until Water has real combat behavior; never present a fake usable Water element.
3. Store permanent unlocked-element flags separately from run upgrades (versioned `USaveGame` or equivalent). Guard save/load failure and duplicate unlocks; restart the app and check persistence.
4. Make the run select its allowed element list from base {Fire, Wind} + permanent unlocks. Reflect unlock availability in visible UI. Rerunnable trials must not repeatedly grant new rewards.
5. Victory/Defeat → results and Retry flow. Cleanup all pending timers, old projectiles, DoT and status snapshots, enemy death bindings, cached input state and temporary modifiers before a new run.
6. Scope authoritative gameplay updates to the existing authority boundaries; do not claim online play implemented.

### D — Validation / documentation
1. Keep previous 45 `WEVORA.` tests from PR #9 passing. Add automated regression for wave spawning/cap and completion, single transition, defeat precedence, upgrade apply/reset, base reactions unaffected, locked-element gating, first-time trial unlock, save/load, replay, timer cleanup.
2. UE 5.8.3 Win64 Development Editor & Game build; compile Blueprint assets; package the solo map plus old maps. Report exact test numbers, warnings, failures, cook results and checksums.
3. If Unreal/Windows are not available, run only feasible static checks, explicitly mark Editor/PIE/Player unverified and DO NOT claim the build or human gameplay passed.
4. Human acceptance later: keyboard/mouse + controller only if already supported, flight → weaving → hits → wave clears → upgrade choices → boss telegraph → trial clear/unlock → new run, plus loss/retry; verify in PIE and packaged Player. Document remaining UX/balance assumptions.
5. Update `PROGRESS.md`, docs and design references. Distinguish design decisions from tunable values. Preserve all prior map/Blueprint assets and existing behavior.

## Existing integration points confirmed on 2026-10-19

- `Source/WEVORA/Playground/WEVORACombatPlayground.{h,cpp}` = original dedicated testing actor, resets all targets and player, F1/F2/F3/R; **do not replace it with the run director**.
- `Source/WEVORA/Playground/WEVORAPlaygroundTarget.h` = moving/respawning test target, derived from existing enemy; **avoid using its auto-respawn behavior as a run-enemy lifecycle**.
- `Source/WEVORA/AI/WEVORAHealthComponent.h` = `OnHealthChanged`, `OnDeath`, `OnReset`, `ResetHealth()`; implement safe healing/max HP modifier without losing baseline.
- `Source/WEVORA/Spell/WEVORASpellWeavingComponent.h` = `AvailableElements`, `ResetCombat()`, charge/cooldown; gate **only for Solo run** and preserve Playground test access.
- Existing Spell Cast, Delivery, Reaction, Zone-local-status and projectile systems remain the source of truth for damage/status.

## Branch and merge strategy

- `main` is 3 documentation commits ahead of PR #9's ancestor (2026-10-17 Solo direction). Reconcile those docs deliberately, **do not revert newer Solo design with old ROADMAP/GAME_DESIGN content**.
- Existing PR #8: `feature/combat-playground` → `main` (Draft).
- Existing PR #9: `feature/localized-elemental-reactions` → #8 branch (Draft).
- This implementation branch starts at PR #9 head. Later create its own Draft PR targeting PR #9 branch, until dependencies are safely merged; only then retarget to main as appropriate. **No forced pushes, no silent merges, no deletion of other branches.**

## Done = practical acceptance

A player can start from a fresh save, finish a reproducible run with at least two upgrade selections and one boss, unlock a genuinely supported element via a trial, restart the executable and find that element available; losing/retrying removes temporary upgrades; existing Combat Playground and localized Fire/Wind reactions still function.
