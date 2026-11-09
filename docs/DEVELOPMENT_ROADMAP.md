# Development Roadmap and Feature Status

Legend: **NS** not started · **IP** in progress · **I** implemented · **AT** automated-tested · **MT** manually tested (screenshot gauntlet) · **HW** hardware verified

| Feature | Status |
|---|---|
| Shared core, fixed-step sim, software renderer | I, AT, MT |
| Windows dual-screen window, input, audio, saves | I, MT (screenshot gauntlet; hand-played session pending) |
| 3DS `.3dsx` build | I, emulator boot verified (Azahar log), not HW |
| 3DS `.cia` packaging | I (CI), awaiting first CI run |
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
| Audio (29 sfx, ambience, music) | I (not listened to by a human yet) |
| GitHub repo + CI | IP |
| Hospital, Grocery, Train Yard, Scrapyard, Woods, Residential maps | NS |
| Expanded weapon catalog beyond 9 | NS |
| Original 3DS performance measurement | NS (needs hardware) |
| FBI QR install | NS (needs release + hardware) |

## Next milestones
1. Get CI green on both platforms and publish v0.1.0 with .zip, .3dsx and .cia.
2. Generate the FBI QR for the released CIA and have a 3DS owner test it.
3. Inspect the 3DS build visually in the emulator and measure it on hardware.
4. Build the remaining six locations (Phase 8), starting with the Grocery Store (low threat, food).
5. Polish: smoother night lighting, more character detail, more weapons.
