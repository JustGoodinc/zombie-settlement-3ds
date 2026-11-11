# Development Roadmap and Feature Status

Legend: **NS** not started · **IP** in progress · **I** implemented · **AT** automated-tested · **MT** manually tested (screenshot gauntlet) · **HW** hardware verified

| Feature | Status |
|---|---|
| Shared core, fixed-step sim, software renderer | I, AT, MT |
| Windows dual-screen window, input, audio, saves | I, MT (screenshot gauntlet; hand-played session pending) |
| 3DS `.3dsx` build | I, emulator boot verified (Azahar log), not HW |
| 3DS `.cia` packaging | I, CI-built and header-checked, published in v0.1.0 |
| Movement, 8-way facing, sprint/dodge, stamina | I, AT (partial), MT |
| Hit probabilities, aim assist, headshots | I, AT |
| Firearms, ammo, reload, durability, no jams | I, AT |
| Melee | I, MT |
| Zombie AI: idle/wander/lying/detect/chase/lose/investigate/doors | I, AT |
| Containers, lime outline, search, loot tables | I, AT, MT |
| Grid inventory: drag, rotate, swap, stack, sort, upgrades | I, AT (logic), MT |
| Inventory pause | I, AT |
| Police Station level | I, AT (reachability), MT |
| Extraction, permanent death, dropped bags | I, AT |
| Loot refresh (15 min active) | I, AT |
| Settlement buildings, upgrades, costs, plots | I, AT, MT |
| Jobs, routines, settler animation | I, MT |
| Security, incidents, journal | I, AT |
| Recruitment, screening, quarantine | I, AT (accept flow), MT |
| Infection stages, treatment, turning | I, AT |
| Day/night, flashlight, line-of-sight visibility | I, MT |
| Squad companions (follow/hold/retreat, combat) | I, MT |
| Survivor rescue escort | I, MT (unit test pending) |
| World map + prep + summary | I, MT |
| Save/load, corruption recovery | I, AT |
| Animation system + test room | I, AT, MT |
| Audio (29 sfx, ambience, seven-piece soundtrack) | I; soundtrack listened to and approved by the owner (2026-11-10); effects and in-game balance not yet reviewed by ear |
| GitHub repo + CI + release v0.1.0 | I (CI green: Windows tests + smoke launch, 3DS build, CIA package) |
| Hospital, Supermarket, Train Yard, Neighborhood maps (five launch locations with the Police Station) | I, AT, MT (bot playtest, 30 runs per map) |
| Visual overhaul (baked level layer, lighting, furniture, menus, icons, title) | I, AT (partial), MT |
| Zombie looks (16 by place x 4 versions, turned survivors) | I, AT, MT |
| Tutorial (opening scene, raid lessons, buildings tour, first upgrade) | I, AT, MT (scripted walkthrough) |
| Expanded weapon catalog beyond 9 | NS |
| Original 3DS performance measurement | NS (needs hardware) |
| FBI QR install | I (QR points at verified public CIA URL), not HW-tested |

## Next milestones
1. ~~CI green + v0.1.0 release~~ done.
2. Have a 3DS owner test the CIA via the FBI QR.
3. Inspect the 3DS build visually in the emulator and measure it on hardware.
4. Listen to the effects and the in-game mix, and tune them by ear.
5. More weapons.
