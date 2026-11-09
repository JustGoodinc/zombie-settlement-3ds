# Session Handover

_Last updated: 2026-11-09 (session 1)_

## Current milestone
Phases 1–7 have a first playable implementation; Phase 10 (CI/release) is in progress. Phase 8 (the remaining six locations) has not started. Detailed status: [DEVELOPMENT_ROADMAP.md](DEVELOPMENT_ROADMAP.md).

## Working (with evidence)
- Windows exe builds (MSVC 14.51, static CRT, 496 KB).
- 3DS `.3dsx` builds (devkitARM GCC 16.1) and boots in Azahar (log evidence only).
- 214/214 automated test checks pass (`build\zs_tests.exe`).
- The 30-shot gauntlet in `screenshots/auto/` has been visually inspected.

## Not verified
- 3DS on real hardware and its performance.
- CIA packaging, which runs only in CI and has not run yet.
- FBI QR install.
- A hand-played PC session with keyboard and mouse. The scripted input path is tested; real input devices are not.

## Build commands
```bat
scripts\build_windows.bat & build\zs_tests.exe
build\zs_tool.exe --shots screenshots\auto   & rem visual gauntlet
build\zs_tool.exe --gallery assets\gallery   & rem asset review sheets
build\zs_tool.exe --bench
```
```bash
export DEVKITPRO=/c/devkitPro DEVKITARM=/c/devkitPro/devkitARM
/c/devkitPro/msys2/usr/bin/make        # 3DS
```
Azahar emulator: `C:\Users\admin\Tools\Azahar\azahar-windows-msvc-2126.2\azahar.exe ZombieSettlement.3dsx`.

## Key design decisions
- A shared software renderer instead of SDL2 or citro2d: pixel-identical output, no downloads, headless tests and screenshots.
- 3/4 top-down presentation following Try 2 (walls with front faces). 16 px tiles and characters, 8 visual directions (3 mirrored).
- Saving only at the settlement, with autosave on deploy, death, extraction and each morning. Quitting mid-expedition sends the squad home empty-handed.
- The loot refresh timer starts on departure (see WORLD_MAP_SPECIFICATION).

## Immediate next actions
1. Push, watch both workflows, and fix whatever fails (most likely `cia.rsf` or the bannertool paths).
2. Tag `v0.1.0`, create the release with zip/3dsx/cia plus SHA-256 sums, then generate `ZombieSettlement-FBI-QR.png` pointing at the CIA asset URL (the release step needs a QR generator: build one into zs_tool or use the `qrencode` apt package in CI).
3. Ask the owner to test the CIA and QR on hardware, and to report Old 3DS FPS ("Show FPS" in Settings).
4. Phase 8: Grocery Store map (ASCII in `src/world/maps.cpp`, add a `MAP_*` id, set `LOCATIONS[2].map`).
