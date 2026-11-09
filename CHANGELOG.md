# Changelog

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
