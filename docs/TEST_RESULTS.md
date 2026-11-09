# Test Results

Run: `build\zs_tests.exe` (MSVC Release, 2026-11-09). **214 passed, 0 failed.**

| Area | What is checked |
|---|---|
| Combat | baseline table exactly 300/400/150/100/50; 20 000 random parameter sets always sum to 1000 with no negatives; 200 000-roll distribution within 0.6% of the table; distance, skill and movement modifiers; damage per result; headshot instant kill; leg slow; miss does no damage; ammo and durability use; broken guns don't fire; empty gun auto-reloads; knife has infinite durability; projectile outcome matches the roll |
| Inventory | bounds, overlap, rotation, stacking to max, overflow returns the remainder, backpack upgrade resize, impossible shrink leaves it unchanged, sort |
| Pause | 3 s of input with the inventory open: zombies, survivors, clock and active time all frozen; resume advances them |
| Looting | Take All never deletes items; deterministic loot; guaranteed keycard |
| Maps | Police Station size, container count, zombie and rescue spawns, extraction tile, every container reachable; settlement has 6 plots |
| Zombies | detection → chase, closing distance, lying zombies stay down until close, door battered until broken, gunshot noise draws a distant zombie |
| Settlement | costs deducted, population cap per Quarters level, guards raise security, 200 nights with zero deaths at security ≥ 25, cap enforced |
| Survivors | infection Early → Advanced at 20 h → death; treatment cures Early and costs medicine; skill progression |
| Expeditions | extraction delivers food; squad flags cleared; 15-minute refresh timer starts on departure; refresh clears containers and kills but keeps doors; companion death drops a bag |
| Save | round trip of rotations, deaths, building levels, timers, clock and container contents; truncated payload rejected without side effects; corrupted file restored from backup; stray tmp ignored; delete |
| Animation | every definition valid; frame-rate independent (30 vs 120 Hz); non-interruptible melee; death is final; reload returns to idle; walk is not reset by re-play; dir8 mapping; all 8 directions × 14 poses for survivors and zombies; pivot stability |
| UI | every panel opens and B backs out; the recruit Accept flow |

## Screenshot gauntlet
`zs_tool --shots screenshots/auto` plays a scripted session and captures 30 native-resolution composites (title → settlement → menus → deploy → police → search → loot → inventory → combat → night → pause → squad → extraction → settlement night → infirmary → workshop → recruit → journal → settings → animation room → save/load → main menu). These were inspected visually. One bug was found and fixed: the A press that triggered extraction also activated the summary's button.
