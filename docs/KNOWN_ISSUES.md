# Known Issues and Limitations

- **No real-hardware testing.** Old 3DS frame rate, level-load pauses and memory under load are unmeasured. Every performance figure in these docs comes from the PC benchmark. This needs a hardware measurement before any optimisation decisions.
- **FBI QR install is unverified.** Old FBI builds may fail TLS on GitHub's CDN redirect; the manual install fallback is documented.
- **Emulator visuals not inspected.** The Azahar boot was confirmed from its log only (the build starts and keeps running; nobody has looked at its picture or listened to its sound).
- **Audio is only partly reviewed by ear.** The owner listened to all seven pieces of the soundtrack (the exported loops) on 2026-11-10 and approved them. The effects, the ambience, and how the music balances against them in play have still only been checked by measurement.
- The tutorial's first raid protects you only from the two zombies of the fight lesson (weak bites, no infection). Walk further into the police station and everything else there plays by the normal rules, permanent death included.
- The 45° character directions reuse front/back sprites with a turned head.
- Quitting mid-raid: on reload the squad returns home empty-handed with −15 HP. Deaths that happened before quitting stay saved.
- The trapped survivor's HP is not persisted between visits.
