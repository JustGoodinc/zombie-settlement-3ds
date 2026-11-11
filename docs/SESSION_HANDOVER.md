# Session Handover

_Last updated: 2026-11-09 (session 1)_

## Current milestone
Phases 1–7 have a first playable implementation; Phase 10 (CI/release) is done for v0.1.0. Phase 8 (the remaining six locations) has not started. Detailed status: [DEVELOPMENT_ROADMAP.md](DEVELOPMENT_ROADMAP.md).

## Working (with evidence)
- Windows exe builds (MSVC 14.51, static CRT, 496 KB).
- 3DS `.3dsx` builds (devkitARM GCC 16.1) and boots in Azahar (log evidence only).
- 214/214 automated test checks pass (`build\zs_tests.exe`).
- The 30-shot gauntlet in `screenshots/auto/` has been visually inspected.

## Released
- v0.2.0 (latest; D-pad/Circle Pad in every menu, backpack cursor fix, thumb-rail bottom screen): https://github.com/JustGoodinc/zombie-settlement-3ds/releases/tag/v0.2.0. v0.1.3 = readable controls, tutorial, camera zoom. v0.1.2 = CIA loader fix. v0.1.1 = Try 2 visual pass (its CIA crashes the loader, do not use), v0.1.0 = first slice. Each has zip, 3dsx, cia, SHA256SUMS, FBI QR. CI is a single workflow `.github/workflows/ci.yml`; pushing a `v*` tag publishes a release.

## Not verified
- 3DS on real hardware and its performance.
- FBI QR install on real hardware.
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
- Saving only at the settlement, with autosave on deploy, death, extraction and each morning. Quitting mid-raid sends the squad home empty-handed.
- The loot refresh timer starts on departure (see WORLD_MAP_SPECIFICATION).

## Immediate next actions
1. Owner hardware test: install via the FBI QR, report Old 3DS FPS ("Show FPS" in Settings); record in TEST_RESULTS and PERFORMANCE_RESULTS.
2. Inspect the 3DS build visually in Azahar (the owner declined screen control once; ask first).
3. Phase 8: Grocery Store map (ASCII in `src/world/maps.cpp`, add a `MAP_*` id, set `LOCATIONS[2].map`).
