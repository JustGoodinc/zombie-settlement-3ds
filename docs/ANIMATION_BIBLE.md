# Animation Bible

## System
- `src/core/anim.h/.cpp`: `Animator` state machine (state, frame, time, per-instance speed). Definitions are data in `data/anims.inc`; a static_assert forces one entry per state.
- Frames have variable durations (10 ms units), a **pose**, a **kick** (render-only offset along facing, used for recoil, lunges and swings), a **bob**, and **event bits**: FOOTSTEP, MUZZLE, IMPACT, RELOAD, SWING, BODYFALL, GROAN, DONE.
- Priorities and interruptibility: a non-looping, non-interruptible action (Melee, Dodge, Death, ZAttack, ZRise) can only be replaced by a higher priority. Death/Dead can never be left (no revivals). Replaying the current state never resets it, so rapid direction changes keep the walk cycle.
- Timing runs on simulation time only (30 Hz fixed step). Render positions interpolate between ticks. Tested: identical footstep counts and frames at 30 vs 120 updates/s.
- Gameplay never waits on animation: facing follows the Circle Pad instantly, firing is never delayed (shooting while moving keeps the walk legs and adds muzzle/recoil), and a dodge cancels a reload with no ammo lost.
- Melee hits and zombie bites apply on their IMPACT frame. A dead zombie's death animation plays on while it is already flagged dead and harmless.

## Sprites and directions
16×16 characters with a stable pivot at the feet (tested: lowest opaque row identical between stand and walk). 5 directions are stored (S, SE, E, NE, N); W, SW and NW are mirrored, giving 8 visual directions. 14 generated poses per direction per variant: STAND, BREATHE, WALK_A/B, RUN_A/B, CROUCH, REACH, LEAN_BACK, LEAN_FWD, HURT, FALL, DEAD, LOOK. The held weapon is drawn as a line along the exact facing vector (continuous aim), with a swing arc for melee and a lowered angle for reloads.

## Animation list
| State | Frames | Cycle (ms) | Loop | Events |
|---|---|---|---|---|
| Idle | 6 | 4600 | ✓ | breathing + occasional head turn |
| Walk | 4 | 480 | ✓ | FOOTSTEP ×2 |
| Run | 4 | 320 | ✓ | FOOTSTEP ×2, forward lean |
| Aim | 1 | 500 | ✓ | |
| Shoot | 3 | 170 | | MUZZLE, recoil kick −2 px |
| Reload | 4 | 1000 (scaled by weapon reload time in gameplay) | | RELOAD ×2 |
| Melee | 4 | 360 | | anticipation, SWING, IMPACT, recovery |
| Dodge | 3 | 240 | | FOOTSTEP |
| Search | 2 | 500 | ✓ | |
| Hurt | 2 | 180 | | directional knock-back kick |
| Heal | 3 | 750 | | |
| Death → Dead | 3 | 380 | | BODYFALL, persistent body |
| ZIdle | 4 | 3300 | ✓ | GROAN (desynced per zombie) |
| ZWalk | 4 | 840 | ✓ | FOOTSTEP, heavy lurch |
| ZDetect | 2 | 400 | | GROAN |
| ZAttack | 3 | 540 | | IMPACT (bite lands here) |
| ZHurt | 2 | 180 | | |
| ZStumble | 3 | 460 | | (leg shots) |
| ZDeath → Dead | 3 | 320 | | BODYFALL |
| ZLying | 1 | – | ✓ | |
| ZRise | 3 | 750 | | GROAN |
| Work / Sleep / Guard | 4 / 2 / 4 | 1200 / 4000 / 4800 | ✓ | settlement routines |

Zombies get a random speed (0.85–1.15) and start offset so groups never move in sync.

## Effects
Muzzle flash, ejected casings with a tink sound, tracers, blood spray and floor decals, sparks and dust on wall hits, door splinters, window glass, hit flash, stumble, restrained configurable screen shake. Headshots get a HEADSHOT label; misses get MISS and never a blood effect.

## Environment
Swaying tree canopies, 3-frame campfire with embers and smoke, a vibrating generator with a status light, flickering lamps (light sources), a blinking radio light, doors that animate open and closed and wobble when battered, crack overlays, broken windows, drifting leaves in daylight, sleeping settlers with bedrolls and "z", building construction dust and scaffold flash, damaged-building smoke.

## UI
Panels slide in over 150 ms (ease-out), the stylus ghost follows immediately, the drop preview is green or red, the button press depresses by 1 px, toasts fade, and the cursor pulses. UI animation runs while the world is paused.

## Debug
Animation Test Room (Settings → Animation Test Room): any state on survivors or zombies in all 8 directions plus a 6× zoom, variant switch, 0.25×–1× speed, frame and event readout.

## Status and known visual issues
- Implemented and automated-tested: state rules, timing independence, all poses present, pivot stability.
- Not hardware-verified. The 45° directions reuse front/back art with a turned head, a deliberate simplification at 16 px. Lighting cells are 8 px, so night edges look blocky.
