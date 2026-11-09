# Windows Build

Requirements: Visual Studio 2022+ with the C++ workload (tested with MSVC 14.51), Windows 10/11 x64. No third-party libraries.

```bat
scripts\build_windows.bat          :: build\ZombieSettlement.exe, zs_tests.exe, zs_tool.exe
build\zs_tests.exe                 :: automated tests (exit code = failures)
build\zs_tool.exe --shots screenshots\auto
```
CMake alternative: `cmake -S . -B build -A x64 && cmake --build build --config Release`.

The executable links the static CRT (`/MT`), so players need no runtime installer. Saves, `settings.ini` and `controls.ini` live in `%APPDATA%\ZombieSettlement\`.

CI (`.github/workflows/windows.yml`) builds, runs the tests and the benchmark, zips the game, and launches the zipped exe from outside the build directory as a smoke test.
