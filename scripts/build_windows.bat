@echo off
rem Builds the Windows game, tests and tool with MSVC directly (no CMake needed).
rem Usage: scripts\build_windows.bat [release|debug]
setlocal
cd /d "%~dp0\.."
if not defined VCINSTALLDIR (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath`) do set VSDIR=%%i
  call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)
set MODE=%1
set FLAGS=/nologo /std:c++17 /EHsc /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /MT
if /i "%MODE%"=="debug" (set FLAGS=%FLAGS% /Od /Zi /MTd) else (set FLAGS=%FLAGS% /O2 /DNDEBUG)
if not exist build\obj mkdir build\obj
set CORE=src\core\*.cpp src\gameplay\*.cpp src\world\*.cpp src\settlement\*.cpp src\ui\*.cpp
cl %FLAGS% /c %CORE% /Fobuild\obj\ || exit /b 1
lib /nologo /OUT:build\zs_core.lib build\obj\*.obj || exit /b 1
rc /nologo /fo build\app.res src\platform\windows\app.rc || exit /b 1
cl %FLAGS% src\platform\windows\main_win.cpp build\app.res build\zs_core.lib winmm.lib user32.lib gdi32.lib shell32.lib /Fobuild\ /Febuild\ZombieSettlement.exe /link /SUBSYSTEM:WINDOWS || exit /b 1
cl %FLAGS% tests\tests.cpp build\zs_core.lib /Fobuild\ /Febuild\zs_tests.exe || exit /b 1
cl %FLAGS% tools\zs_tool.cpp build\zs_core.lib /Fobuild\ /Febuild\zs_tool.exe || exit /b 1
echo BUILD OK
