# Gameplay Systems

Code map: `src/gameplay/game.cpp` (simulation), `combat.cpp`, `inventory.cpp`, `content.cpp` (data), `src/world/` (maps, collision, LOS, flow fields), `src/settlement/` (economy), `src/ui/` (menus), `src/core/` (renderer, art, audio, animation, saves).

## Simulation
- Fixed step: 30 Hz (`DT`). Rendering interpolates positions with `g_renderAlpha`, so the PC can render at 60 FPS without speeding anything up.
- Any panel other than the HUD pauses the world: no AI, projectiles, clock or loot timers. UI animations keep running.
- One game day is 30 real minutes by default (Settings: 15/30/60). Hour ticks drive infection, healing and incidents; 06:00 drives the daily economy.

## Zombies
States: Idle, Wander, Lying, Rising, Chase, Investigate, Dead.
- Sight: 150 px with line of sight, within a forward cone (or any direction under 44 px). Lying zombies rise when a survivor comes within 44 px.
- Hearing: every gunshot has a noise radius (260–420 px). Zombies inside it investigate the spot. Looting is silent.
- They lose track after 4 s without sight, then investigate the last known spot.
- Pathing: a BFS flow field from the player, rebuilt every 0.5 s, which routes *through* closed doors and windows. A zombie blocked by a barrier attacks it (8 damage per 1.3 s); the barrier shows cracks below 66% and 33% and breaks at 0.
- Performance: zombies farther than 300 px update every 4th tick; separation uses a 64 px spatial hash.

## Companions
Follow (stay within about 28 px, flow-field pathing that respects closed doors), Hold, Retreat (to extraction). They engage visible zombies within 120 px using the same hit system and their own skills and ammo. Stuck or far-away companions regroup when out of sight.

## Doors
Closing a door locks it against zombies. Keycard doors need the Station Keycard; cell doors need the Cell Key. Doors never close on an occupied tile.

## Visibility and lighting
Per-tile light: day/indoor base, a personal radius, a flashlight cone (±30°, 160 px, smoothly rotating), and lamps and campfire with flicker. Tiles without line of sight are dimmed to at most 48. A tile counts as visible at ≥ 60 light; zombies are drawn and targetable by companions only on visible tiles. Lighting is drawn as 8×8 cells blended with neighbours.
