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
- Real hardware: v0.1.1 CIA crashed in the system `loader` on a New 3DS XL (Luma3DS panic). Cause: invalid CpuSpeed (268MHz) and missing MemoryType in my hand-written cia.rsf. v0.1.2 uses the ftpd-based spec; awaiting a retest. Frame time and memory on an Old 3DS are unmeasured.
