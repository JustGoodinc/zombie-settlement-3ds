# Technical Architecture

```
src/core        renderer (RGB565 software canvases), font, PNG writer, art generation,
                audio synth + mixer, animation controller, save format
src/gameplay    game state + fixed-step simulation, combat, inventory, content database, world renderer
src/world       tile maps (ASCII layouts), collision, line of sight, BFS flow fields
src/settlement  economy, incidents, infection, recruitment, loot tables
src/ui          immediate-mode widgets + every bottom-screen menu
src/platform/windows  Win32 window, GDI present, input, XInput, waveOut
src/platform/n3ds     libctru framebuffers, HID, NDSP, SD paths
data/anims.inc  animation definitions (data file)
```

**Why a software renderer:** both platforms draw into identical 400×240 and 320×240 RGB565 buffers. The PC presents them with `StretchDIBits`; the 3DS copies them into its rotated framebuffers (`GSP_RGB565_OES`). Output is pixel-identical, there's no GPU dependency and no SDL download, and screenshots and tests run headless. Budget: a full scene renders in about 0.6 ms on a desktop core; Old 3DS timing is not yet measured on hardware (see PERFORMANCE_RESULTS).

**Platform interface:** platforms only provide `Input` (buttons, Circle Pad, touch), call `game_frame(input, dt)` and `game_render(top, bot, alpha)`, pull audio with `audio_mix`, and set the save directory. Files use portable stdio on both (`sdmc:/3ds/ZombieSettlement/` on 3DS, `%APPDATA%\ZombieSettlement\` on PC).

**Memory:** all pools are fixed (72 zombies, 48 projectiles, 192 particles, 4 units). No allocation in the main loop. Sprites are allocated once at startup. The 3DS ELF uses about 2.2 MB (text plus bss), which fits the Old 3DS 64 MB application mode with a wide margin.

**Determinism:** xorshift RNG with explicit seeds. Loot is seeded from (location, container, generation).

**Saves:** see [SESSION_HANDOVER.md](SESSION_HANDOVER.md). The format is `ZSAV | version | length | payload | CRC32`, with explicit little-endian field serialisation (no struct dumps). Write sequence: write a tmp file, read it back and verify, rotate the old save to `.bak`, then rename. Loading falls back to `.bak`, and a corrupt save never overwrites the running game.
