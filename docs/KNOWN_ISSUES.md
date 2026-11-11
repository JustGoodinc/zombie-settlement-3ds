# Known Issues and Limitations

- **Old 3DS frame rate.** The owner ran v0.4.0 on an Old 3DS XL at about 20 fps. The next version adds Fast graphics (Auto picks it on an Old 3DS) and roughly halves the frame cost in the emulator's Old 3DS mode, but whether that holds 30 fps on a console is not yet measured. The game can measure itself on a console (see docs/NINTENDO_3DS_BUILD.md). Level-load pauses and memory under load on a console are also unmeasured.
- When a squad member dies, the game stalls for one frame (about 0.14 s in the emulator's Old 3DS mode) because permanent death is saved to the SD card at once.
- **FBI QR install is unverified.** Old FBI builds may fail TLS on GitHub's CDN redirect; the manual install fallback is documented.
- **Emulator visuals not inspected.** The Azahar boot was confirmed from its log only (the build starts and keeps running; nobody has looked at its picture or listened to its sound).
- **Audio is only partly reviewed by ear.** The soundtrack was rewritten on 2026-11-10 (slow, horror) after the owner listened to the first version and found it too upbeat. The owner then listened to the seven new pieces (the exported loops) and approved them the same day. The effects, the ambience, and how the music balances against them in play have still only been checked by measurement, and nothing has been heard on a 3DS.
- **The new soundtrack costs more to play than the first.** On the PC benchmark the music takes 0.67 ms of processor time per second of sound (the first soundtrack took 0.29 ms). The whole mix, at 0.87 ms, is still below the 1.09 ms of the mixer in v0.3.0, but none of this has been measured on a 3DS.
- Some of the pieces carry sounds that could be taken for the game's own: a door slamming far off in the police station, knocking in the supermarket, footsteps overhead in the neighbourhood, a thud outside the walls at the settlement. They are quiet and low and meant to unsettle; if they mislead in play, each is one pattern in `src/core/music.cpp`.
- The tutorial's first raid protects you only from the two zombies of the fight lesson (weak bites, no infection). Walk further into the police station and everything else there plays by the normal rules, permanent death included.
- The 45° character directions reuse front/back sprites with a turned head.
- Quitting mid-raid: on reload the squad returns home empty-handed with −15 HP. Deaths that happened before quitting stay saved.
- The trapped survivor's HP is not persisted between visits.
