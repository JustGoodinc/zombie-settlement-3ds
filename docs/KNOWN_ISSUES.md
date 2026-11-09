# Known Issues and Limitations

- **Six of seven locations are not built.** They appear on the world map as UNSCOUTED with Deploy disabled.
- **No real-hardware testing.** Old 3DS frame rate and memory under load are unmeasured. The PC proxy benchmark is 0.6 ms/frame, and my rough guess for an Old 3DS is 18–36 ms (around the 30 FPS budget). This needs a hardware measurement before any optimisation decisions.
- **FBI QR install is unverified.** Old FBI builds may fail TLS on GitHub's CDN redirect; the manual install fallback is documented.
- **Emulator visuals not inspected.** The Azahar boot was confirmed from its log only.
- Night lighting uses 8 px cells, so edges look stepped.
- The 45° character directions reuse front/back sprites with a turned head.
- Character sprites are simpler than the Try 2 mockup characters (16 px readability trade-off).
- Quitting mid-expedition: on reload the squad returns home empty-handed with −15 HP. Deaths that happened before quitting stay saved.
- The trapped survivor's HP is not persisted between visits.
- Audio has not been reviewed by ear.
