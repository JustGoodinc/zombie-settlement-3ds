# Performance Results

## PC proxy benchmark (`zs_tool --bench`)
Scene: Police Station, night, flashlight on, 60 zombies chasing, 3 survivors firing, 240 frames.
Machine: development PC (Windows 11, MSVC /O2). Date: 2026-11-09.

| Metric | Value |
|---|---|
| Simulation | 0.020 ms/frame avg |
| Render (world + light + HUD + bottom UI) | 0.608 ms/frame avg |
| Worst frame | 1.245 ms |
| Game state | 274 KB |
| Frame buffers | 337 KB (PC composite) |

## Nintendo 3DS
| Metric | Value |
|---|---|
| ELF text + data + bss | 341 KB + 9 KB + 1.93 MB ≈ 2.2 MB |
| Frame time | **not measured** (needs hardware; `G.frameMs` CPU time is shown with "Show FPS" in Settings) |

Built-in budget controls: zombies far from the player update at 1/4 rate; fixed pools (72 zombies, 192 particles, 48 projectiles, 8 audio voices); lighting is computed per tile (about 450 rays a frame), never per pixel.
