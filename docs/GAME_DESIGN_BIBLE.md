# Game Design Bible

The authority for design decisions. Visual authority: [TRY_2_VISUAL_SPECIFICATION.md](TRY_2_VISUAL_SPECIFICATION.md).

**Loop:** Settlement → expedition prep → location → scavenge → fight or avoid → extract → deliver → upgrade → repeat.

**Pillars**
1. Tense, readable top-down survival on a small screen. Night is dangerous because you cannot see, not because zombies get stronger.
2. Every survivor matters: permanent death, individual skills, packs and infections.
3. The backpack is a puzzle: grid space, rotation and stacking decide what comes home.
4. The settlement is a living base: jobs, routines, incidents and visible upgrades.

**Fixed rules** (from the handover, implemented):
- Only ordinary zombies, with idle, wandering and lying variants. No special infected.
- Headshots instantly kill ordinary zombies. Weapons never jam. Durability never degrades damage or accuracy; at 0 the weapon is unusable until repaired. Knives never wear out.
- Gunfire makes noise; looting is silent. Melee, sprint and dodge cost stamina; gunfire does not.
- Opening any menu (inventory, containers, settlement menus) pauses the simulation, including the world clock.
- One survivor is controlled, plus up to 2 AI companions per expedition.
- Population starts at 5, capped at 20 (Living Quarters level 4).
- Loot refreshes after 15 minutes of active play (see the rule in [WORLD_MAP_SPECIFICATION.md](WORLD_MAP_SPECIFICATION.md)).
- Saving is only allowed at the settlement. The game autosaves on deploy, on every death, on extraction, each morning, and after construction.

System specs: [GAMEPLAY_SYSTEMS.md](GAMEPLAY_SYSTEMS.md), [COMBAT_SPECIFICATION.md](COMBAT_SPECIFICATION.md), [INVENTORY_SPECIFICATION.md](INVENTORY_SPECIFICATION.md), [SETTLEMENT_SPECIFICATION.md](SETTLEMENT_SPECIFICATION.md), [SURVIVOR_SPECIFICATION.md](SURVIVOR_SPECIFICATION.md), [WORLD_MAP_SPECIFICATION.md](WORLD_MAP_SPECIFICATION.md), [ANIMATION_BIBLE.md](ANIMATION_BIBLE.md).
