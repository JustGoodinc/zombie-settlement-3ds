# Known Issues and Limitations

- **No real-hardware testing.** Old 3DS frame rate, level-load pauses and memory under load are unmeasured. Every performance figure in these docs comes from the PC benchmark. This needs a hardware measurement before any optimisation decisions.
- **FBI QR install is unverified.** Old FBI builds may fail TLS on GitHub's CDN redirect; the manual install fallback is documented.
- **Emulator visuals not inspected.** The Azahar boot was confirmed from its log only (the build starts and keeps running; nobody has looked at its picture or listened to its sound).
- **Audio has not been reviewed by ear.** The effects, the ambience and the soundtrack were checked by measurement only (levels, no clipping, the notes that sound, how each piece builds). How the music actually sounds, and how it balances against the effects, needs a listener.
- The tutorial's first raid protects you only from the two zombies of the fight lesson (weak bites, no infection). Walk further into the police station and everything else there plays by the normal rules, permanent death included.
- The 45° character directions reuse front/back sprites with a turned head.
- Quitting mid-raid: on reload the squad returns home empty-handed with −15 HP. Deaths that happened before quitting stay saved.
- The trapped survivor's HP is not persisted between visits.
