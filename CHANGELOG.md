# Changelog

## Unreleased — readability, tutorial, zoom
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
