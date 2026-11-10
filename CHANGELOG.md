# Changelog

## Unreleased — settlement performance, C material menus
- The settlement renders 56% faster (0.64 -> 0.28 ms/frame on the PC benchmark; the police station 0.27 -> 0.19), aimed at a steady 30 fps on 3DS: no integer divisions in the sprite blitter (the 3DS CPU has no hardware divide), a packed one-multiply 565 blend for lighting and overlays, the settlement colour grade through a lookup table, static lights baked per tile, the player's line of sight cached per tile, and light pools from cached falloff masks. Visual output is unchanged.
- `zs_tool --bench-base` benchmarks the settlement (day / dusk / night) with a per-stage breakdown.
- Bottom-screen menus use the v3 "C" material kit everywhere: charcoal woven cloth with a stitched hem and a stitched seam above the prompt footer, leather corner patches with brass rivets, stitched cloth side rails, muted wood action buttons, flat charcoal disabled buttons, olive-gold active choices, and gold corner brackets on the focused button so focus reads by shape as well as colour. Panels are flat dark data fields with a slate title strip. The empty backpack info box shows a small canvas strap in its spare margin. Layout, touch targets and navigation are unchanged; the UI greys sit exactly on RGB565 levels so they stay neutral on 3DS.
- Marcus and Javier wear the red cap badge in the world as well as in their portraits (character lock v3).

## v0.2.1 — roomier settlement, bigger loot screen
- Settlement compound is wider and taller, with at least four tiles of open ground between buildings.
- Container / stash screen: the "Pick loot above" text box is gone; the backpack grid now uses the full space below the loot slots at the same 24 px cells as the backpack screen. The selected item's name shows on the line above the grid.

## v0.2.0 — Staying Alive: Try 2 redesign
- Compared against the mockup panel by panel (settlement, combat, characters, portraits) and iterated: Try 2 compound layout framed fence to fence, grey picket fence, warm dirt yard, big army tent, stone fire pit, iron lamp posts, warm evening light pools, settlers gathering at the fire; red-brick police station facade with glass doors and a lit POLICE sign, 3/4 police car, bold HUD (framed portrait, thick bars, big ammo digits), big blood splats, long tracers, readable nights (render only; stealth unchanged); 3/4 portraits with cap brims and pack straps; cleaner zombies with blood streaks.
- The game is now called **Staying Alive** (title screen, window, 3DS banner/HOME menu name, release file names). Saves stay where they were, and the 3DS title ID is unchanged so the CIA installs over the old one.
- Characters redrawn to match the Try 2 mockup side by side: stocky chibi build (broad shoulders, thick arms, big hands and boots), serious faces with dark eyes under heavy brows, caps with brims jutting to one side. Side view is a 3/4 view like the mockup.
- Animation is built from hand-drawn parts (head, torso/arms, legs): real walk and run cycles with arm swing and steps, a new aim pose with the arm extended, crouch/reach with bent legs.
- Zombies: pale and balding with dark eyes, a gaping bloody mouth, blood on the scalp and torn, bloodied clothes; they reach forward when seen side-on.
- Weapons are pixel-art sprites held in the hand (pistol, revolver, SMG, shotgun, rifle, knife, bat, crowbar, machete) with a big starburst muzzle flash.
- Default camera zoom is now 200% (the mockup framing). Saved settings keep their zoom; change it in Settings.
- Portraits redrawn in the same style (big glinting eyes, brows, blush, caps with brims and emblems, long hair).
- Settlement facelift: a compact walled compound where each facility has its own shape that grows with its level: slate-roofed house with lit windows and dormers, lamp-lit watchtower with railing, sandbags and a pennant, red-cross canvas infirmary tent, tin-roof workshop shed with tools and workbench, stacked shipping containers, greenhouse with a smoking cookhouse. Chimney smoke, window glow and flicker after dusk, fireflies, a stone fire pit with sparks, lamp posts. The settlement camera frames one zoom step wider than expeditions.
- Every menu uses thumb rails: tabs on the left edge, actions on the right edge with Back at the bottom, content in the middle. Includes the desk/crate/stash screen (Take All / Take / Store on the left, Inspect or Donate / Rotate / Back on the right). D-Pad / Circle Pad: push off either side of a backpack grid to reach that rail; up from the grid reaches the loot slots.

## v0.1.4 — menu controls and thumb layout
- D-Pad and Circle Pad both move through every paused menu; holding either repeats.
- Backpack and container grids: D-Pad / Circle Pad move the cursor (fixed: the D-Pad was being swallowed there), A picks up / places, Y rotates, X uses. Push right past the bag to reach the action buttons; Left or B goes back to the grid.
- Gameplay bottom screen rebuilt around thumb rails: tall buttons down the left and right edges (Base, Survivors, Expedition / Journal, Backpack, Pause; on expeditions Backpack, Squad / Flashlight, Pause), with their pad shortcut shown. Map and objective sit in the middle.
- Backpack and container action buttons are taller and sit on the right edge.

## v0.1.3 — readability, tutorial, zoom
- Button prompts redrawn: coloured 3DS buttons (A red, B yellow, X blue, Y green), shoulder/START/SELECT pills, D-pad and Circle Pad icons. On PC they show your actual keys from controls.ini (keycaps) and switch to Xbox labels when a gamepad is used.
- Controls screen is now a glyph + action table; bigger footer prompts; HUD hints use glyphs.
- Skippable tutorial (10 steps: move, backpack, plan, deploy, search, loot, fire, reload, sprint/dodge, extract). Offered on New Game; each step completes from the real action; the button to press pulses; skip from the HUD or Pause; replay from Settings.
- Camera zoom setting (100/125/150/200%, default 125%). Text and prompts are drawn after scaling so they stay sharp.
- settings.ini gains zoom + tutorial flag (older files still load).

## v0.1.2 — CIA fix
- Fix: the v0.1.1 CIA crashed the 3DS system loader on real hardware. The makerom spec is now based on the hardware-proven ftpd template (valid CpuSpeed, MemoryType and memory permissions).

## v0.1.1 — Try 2 visual pass
- Characters rebuilt at 16×24 (faces, shading, backpacks; zombies hunched with torn clothes and red eyes).
- Warm colour grade, vignette, drop shadows, wall ambient occlusion, scattered ground detail.
- Settlement rebuilt: grass and paths, tents, campfire circle, torches, barrels, sandbags, log piles; you start by the fire.
- Police Station: lit POLICE sign with strobes, traffic cones, barrels.
- UI: bevelled metal panels, gradient title bars, bold labels, segmented bars, icons in building, location, survivor and patient lists, 3× inventory cells for small packs, settlement minimap shows buildings.

## v0.1.0 — first playable vertical slice
- Shared C++17 core with a software RGB565 renderer (pixel-identical on PC and 3DS).
- Windows: Win32 dual-screen window, mouse touchscreen, keyboard plus rebinding, XInput, waveOut audio, fullscreen and scaling.
- 3DS: libctru framebuffers, Circle Pad, touch, NDSP audio, SD saves; `.3dsx` and `.cia` packaging.
- Police Station handcrafted level (67 containers, 19 zombies, keycard and cell doors, rescue, extraction).
- Settlement: 6 buildings with upgrades, jobs, economy, security, incidents, recruitment, infirmary, workshop, stash.
- Grid inventory (drag, rotate, swap, stack, backpack upgrades) that pauses the world.
- Probability-based combat (miss/body/arm/leg/head, headshots kill), durability (no jams), melee and stamina.
- Day/night with flashlight cone, line-of-sight visibility, infection stages, permanent death, turning.
- Data-driven animation system (25 states, events, frame-rate independent), animation test room.
- Versioned CRC-checked saves with atomic replace and backup recovery.
- 214 automated test checks, a headless screenshot gauntlet, an asset gallery, and a benchmark.
