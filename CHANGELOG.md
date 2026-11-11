# Changelog

## Unreleased — a new first hour (tutorial, soundtrack, raids) and the visual overhaul
- **New tutorial.** A new game opens with a short scene at the settlement (the fire, the radio, the leader's decision, the walk to the gate, a title card) and goes straight out on a first raid of the police station, alone. Lessons finish when you do the thing, with the button to press drawn in the text and a gold marker on what the lesson points at: move, search the marked car, Take All, open the backpack, fight, reload, dodge, extract. The marked car always holds the same useful first haul. The two zombies of the fight lesson bite weakly and cannot infect, so a first fight cannot end the run; everything else on that raid plays by the normal rules. Back home a second scene takes the camera round the six buildings, then you upgrade one with what you brought back, and the last lesson points at the next raid. A/Next moves a scene on, START skips the scene, Skip Tutorial skips everything. Replay it from Settings.
- **Expeditions are now raids** everywhere in the game (the Raid button, Raid Prep, Raid Failed, the objective and journal lines).
- **A soundtrack.** Seven original pieces for a small synthesiser, in the manner of late-70s and early-80s horror film scores (a steady pulsing bass, one figure that keeps repeating, slow minor chords, sparse drums): Main Title for the menus, Home Fire for the settlement, Precinct Nine (police station), Ward Four (hospital, in 6/8), Aisle Nine (supermarket), Rolling Stock (train yard, in 7/8) and Maple Street (neighbourhood). Each is a loop of 65 to 100 seconds that builds section by section; changing place fades one into the next. The music is played note by note as the game runs (bass, arpeggio, lead, string pad, drum machine, echo), so it adds a few kilobytes and no recordings, and the Music slider still controls it. It replaces the old background drone. The whole mix now costs less than before: 0.46 ms of processor time per second of sound on the PC benchmark against 1.09 ms for the old mixer. Checked by measurement (levels, no clipping at full volume, the notes that sound, how each piece builds), not yet by ear, and not on a 3DS.
- Tools: `zs_tool --tutorial DIR` plays the whole tutorial through the real input and saves every stage; `zs_tool --music DIR` writes each piece as a WAV with a spectrogram and prints what was measured.

### Visual overhaul: one style across the world, menus and icons
- **New look for every map.** Floors, walls, fences, shadows and ground detail are drawn once per level into a baked layer, in clean two- and three-tone pixel clusters instead of per-pixel noise. Every room has its own floor and wallpaper (per location: tan and blue lino with walnut offices in the police station, mint and pale blue in the hospital, cream tiles under a red stripe in the supermarket, a different carpet and wallpaper in every room of every house, steel and concrete in the yard). Walls are dark cut-away tops with a lit rim and a coloured south face; floors darken where they meet a wall; grass, dirt and pavement meet with ragged edges and kerbs.
- **Lived-in detail:** pictures, notice boards and damp on walls, door mats, daylight through windows, weeds along walls, puddles, cracks, tyre marks, manholes, road centre lines, a badge set into the police lobby floor, guide lines down the hospital corridor.
- **Furniture and vehicles redrawn** in the same three-quarter view as the walls (lit top, darker front, navy outline), with variants per place: stocked grocery shelves, medical supplies, evidence boxes, bookshelves, parts bins; checkouts, dressers, hospital beds, waiting-room seats, freezers; cars in six paint jobs; two wrecks.
- **Lighting:** smooth, dithered falloff instead of 8-pixel blocks; shadows fade to navy by day, plum at dusk and deep blue at night; rooms you have seen stay dimly visible; additive glows for lamps and fires.
- **Zombies look like the people they were.** Sixteen looks tied to the place instead of one pale figure in four tints: officers in caps, riot police behind visors and inmates in orange in the police station (whoever is by the cells is an inmate); doctors in white coats, theatre nurses in caps and masks and bandaged patients in gowns in the hospital; shop staff in red caps and aprons among the shoppers in the supermarket; yard crews in hard hats and hi-vis vests and mechanics in beanies and overalls at the train yard; neighbours in dressing gowns, tracksuits, plaid, hoodies and suits. Indoors you meet the place's own people, outside mostly passers-by. Every look comes in four versions (skin tone, wound, hair, and for civilians the colour of the top; hard hats in four colours; one riot visor in four is pushed up), so a group in the same uniform is still different people. A survivor who turns keeps their own hair, clothes and backpack. Looks only: every zombie plays the same.
- **Combat feel** (cosmetic only: own random numbers, no rule or timing changes): muzzle flashes light the room, glowing tracers, smoke, brass casings that bounce and stay, sparks, dust and bullet holes on walls, blood thrown along the shot and onto the wall behind, pools that spread, bodies that stay, camera recoil, a red flash at the screen edge when bitten and a pulse at low health.
- **Atmosphere:** lightbars still turning on some police cruisers, some wrecks still burning, rain on about one day in four (never day 1), exterior building fronts in each location's own material (brick, white panels, stucco, painted clapboard, corrugated steel).
- **Menus and HUD in the same style:** navy slab with sunk data wells, buttons as raised blocks that sink when pressed, gold focus ring with corner brackets, solid gold for the selected choice. Layout, touch targets and navigation are unchanged.
- **Item icons redrawn** at 12 px per inventory cell (was 8), shown at 2x in the starter backpack.
- **Title screen:** dusk skyline, setting sun, the horde on the road, logo with a pink/cyan offset.
- **Settlement laid out as a neighbourhood:** two streets and an avenue from the gate through the fire plaza to the quarters; storage and workshop face the main street by the gate, the watchtower is built into the wall beside the gate, the kitchen sits by its garden, the infirmary in the far corner, tents beside the quarters; grass yards, dirt streets.
- Fires no longer paint over people standing in front of them (the settlement fire pit at dusk, burning wrecks).
- **Faster frames:** police night scene 0.186 -> about 0.16 ms, settlement 0.276 -> about 0.2 ms on the PC benchmark, because floors, walls, decals and furniture shadows are no longer drawn every frame. The level layer takes 3-9 ms to draw once on entering a level (PC) and uses 1.8 MB. Zombie frames are drawn the first time each is shown (about 5 microseconds each on PC) and dropped when a level loads; mixed looks cost no frame time over the old four. None of this has been measured on a 3DS.
- Tools: `zs_tool --mapshots`, `--combat` (a staged firefight in every location), `--settlement`, `--rain`, `--zombies` (look sheets), `--bench-bake`; the handoff now exports baked level layers, every furniture variant and every zombie look. Art direction: `docs/ART_DIRECTION.md`.

## v0.3.0 — five launch maps, playtested
- Five expedition maps at launch: **Police Station**, **Hospital** (wards, surgery, a keycard-locked pharmacy, ER, lobby and ambulance bay), **Supermarket** (loading dock, stockroom, aisles, checkouts, car park), **Train Yard** (harbour quay with shipping containers, three rail lines with boxcars, rail depot warehouse and yard office) and **Neighborhood** (five furnished homes with beds, sofas, fridges and kitchens, fenced yards, a street of parked cars, an abandoned backyard camp, and someone locked in a bedroom with the key in a neighbour's desk). Scrapyard and Woods are retired; old saves still load.
- Each place stocks what it should: supermarket shelves hold food, hospital shelves medicine, home desks and pantries household supplies, freight containers materials, fuel and tools. Most supermarket and depot shelves are picked over, so the stores look full without hundreds of searches.
- New art: rail track, harbour water, shipping containers in four paints, boxcars, beds, sofas, fridges, and HOSPITAL / MARKET / RAIL DEPOT signs.
- Sprint: running out of stamina while holding B now keeps you walking until you let go, instead of stuttering between sprint and walk.
- Movement slides around door frames and furniture corners instead of catching on them.
- Double doors open and close together.
- The interaction prompt prefers what you're facing, so walking into a door and pressing A opens it even with a shelf beside it.
- A trapped survivor in reach always gets the Rescue prompt (a crate or the doorway you're standing in no longer hides it).
- Holding-cell doors can't be smashed by zombies, so the prisoner survives until you bring the key (before, zombies broke in within about 45 seconds). Rescued survivors also start each expedition at full health.
- Companions only walk straight at you when the path is actually clear, and otherwise follow the route around furniture and windows.
- Arrival points no longer have a zombie waiting one step away.
- Searching large props (containers, boxcars) works from any side.
- `zs_tool --playtest DIR [runs]` plays every map through the real game input (all containers, keys, doors, rescue, extraction; solo by day, squad with zombies, and at night) and reports stuck spots, wall scraping, companion lag and 1-tile pinch points with heatmaps. `zs_tool --mapshots DIR` renders overviews and in-game shots of each map. A new test checks every map stays within engine limits with every container reachable.

## v0.2.2 — faster settlement, C material menus
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
