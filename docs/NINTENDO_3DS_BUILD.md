# Nintendo 3DS Build

Requirements: devkitPro with devkitARM and libctru (the `3ds-dev` group).

```bash
make                                   # ZombieSettlement.3dsx (+ .elf, .smdh)
sh scripts/package_3ds.sh <tools dir>  # ZombieSettlement-3DS.cia (needs makerom + bannertool)
```

- `.3dsx`: built by the standard devkitPro 3DS rules from the shared sources plus `src/platform/n3ds/main_3ds.cpp`. Icon and metadata come from `assets/icons/icon.png` and the Makefile `APP_*` fields.
- `.cia`: `bannertool` builds the banner (`assets/icons/banner.png` + `banner.wav`) and SMDH, then `makerom` packages the ELF with `platform/n3ds/cia.rsf`.
  - Unique ID 0xF7A31, product code CTR-H-ZSET, homebrew logo.
  - Spec based on the hardware-proven ftpd template (mtheall/ftpd). SD-only, 64 MB (Old 3DS) application memory mode; the exheader requests 804 MHz, which only New 3DS honours.
  - The game calls `osSetSpeedupEnable` so New 3DS units get 804 MHz. This is optional, not required.
- CI (`.github/workflows/ci.yml`) runs in the `devkitpro/devkitarm` container. It downloads makerom v0.19.0 (3DSGuy/Project_CTR) and bannertool v1.2.2 (Epicpkmn11), packages the files, and checks the 3DSX magic and the CIA header.

## Verification status
- Local build: ✅ `.3dsx` compiles and links, 0 errors (devkitARM GCC 16.1).
- Emulator: ✅ boots in Azahar 2126.2. Log shows the app running, creating `sdmc:/3ds/ZombieSettlement/`, and staying alive with no crash. Visuals in the emulator not yet inspected.
- Real hardware: v0.1.1 CIA crashed in the system `loader` on a New 3DS XL (Luma3DS panic). Cause: invalid CpuSpeed (268MHz) and missing MemoryType in my hand-written cia.rsf. v0.1.2 uses the ftpd-based spec.
- **Old 3DS XL, v0.4.0 (owner, 2026-11-10): it runs, at about 20 fps.** The first measurement on a console. v0.4.1 adds the Graphics setting below and cuts the frame cost; it has not been measured on a console yet.

## Frame rate on the Old 3DS
The game runs at 30 fps when a frame's CPU work stays under 33.3 ms; past that the display waits for the next refresh and it drops to 20 fps. **Settings → Graphics** has three values: Auto (the default: Fast on an Old 3DS, Full on a New 3DS), Full and Fast. Fast:
- shows the settlement at the same zoom as a raid, instead of one step further out (about half the pixels to draw and light);
- skips the settlement's colour grade (a table look-up per pixel);
- while you play, redraws the touch screen every other frame (15 fps), and at once for the stylus, START, SELECT, A and B, so no press is late or lost. Menus are always redrawn.
Everyone also gets a faster 2x scale-up of the world view, a faster copy to the rotated screens, and the zombie sprites of a level drawn while it loads rather than the first time each zombie is seen.

**Measuring a console.** Put an empty file named `bench` in `sdmc:/3ds/ZombieSettlement/` and start the game. Instead of the title screen it plays four fixed scenes (the settlement by day and by night, the police station by day, a night fight with 40 zombies), once with Full and once with Fast graphics, writes `sdmc:/3ds/ZombieSettlement/perf.txt` and quits. Its saves go to `bench_saves/`, never the real ones. Delete `bench` to play again. The file gives the average and worst CPU time per frame, split by stage (simulation, light map, world, shading, colour grade, light pools, scale-up, HUD, touch screen, copy to the screens, sound).

In the Azahar emulator set to Old 3DS (its timing counts instructions, not memory stalls, so a real console is slower: v0.4.0 measured 16-21 ms there but ran at 20 fps on the console, so roughly 2x):

| Scene | v0.4.0 | v0.4.1 Full | v0.4.1 Fast |
|---|---|---|---|
| Settlement, day | 19.3 ms | 16.8 ms | 9.9 ms |
| Settlement, night | 20.9 ms | 18.3 ms | 11.2 ms |
| Police station, day | 16.0 ms | 12.5 ms | 10.4 ms |
| Police station, night fight, 40 zombies | 19.3 ms | 15.5 ms | 13.4 ms |

Loading the police station takes 0.63 s there. A squad member's death stalls one frame (about 0.14 s there) because permanent death is saved to the SD card at once.
