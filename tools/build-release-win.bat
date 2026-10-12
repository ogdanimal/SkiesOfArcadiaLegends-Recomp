@echo off
rem Branded release build (Windows): the launcher, runner and Sys in lib\ModernGekko\build-release,
rem configured for Skies (disc GEAE8P, the widescreen DOL patch applied to the players own main.dol,
rem the game ini). Separate from the dev build: the release runner patches the DOL of any game root it
rem starts. Then package: the module beside the exes and the players' zip (see :package below).
rem usage: tools\build-release-win.bat configure^|build^|package <module.dll> [version]
rem Local Build Tools unless the environment already has MSVC set up (CI: msvc-dev-cmd)
if not defined VCINSTALLDIR (
  call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)
cd /d "%~dp0.." || exit /b 1
set B=lib\ModernGekko\build-release
rem The DOL the launcher must produce: the release patch's output hash (as build-release-linux.sh)
for /f "tokens=2" %%h in ('findstr /b /c:"output-sha256 " release\skies-widescreen.dolpatch') do set DOL_SHA256=%%h
if not defined DOL_SHA256 (
  echo release\skies-widescreen.dolpatch has no output-sha256 line
  exit /b 1
)
if "%1"=="configure" (
  cmake -S lib\ModernGekko -B %B% -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DDOLRECOMP_ENABLE_LLVM=OFF ^
    "-DMODERNGEKKO_FRONTEND_NAME=Skies of Arcadia Legends" "-DMODERNGEKKO_LAUNCHER_OUTPUT_NAME=Skies of Arcadia Legends" ^
    -DMODERNGEKKO_RUNNER_OUTPUT_NAME=skies-run "-DMODERNGEKKO_USER_DIRECTORY_NAME=Skies of Arcadia Legends" ^
    -DMODERNGEKKO_REQUIRED_DISC_ID=GEAE8P ^
    -DMODERNGEKKO_REQUIRED_DOL_SHA256=%DOL_SHA256% ^
    "-DMODERNGEKKO_DOL_PATCH_MANIFEST=%CD%\release\skies-widescreen.dolpatch" ^
    "-DMODERNGEKKO_GAME_SETTINGS_OVERRIDE=%CD%\release\GEAE8P.ini" ^
    "-DMODERNGEKKO_DEFAULT_WINDOW_TITLE=Skies of Arcadia Legends" -DMODERNGEKKO_GAMECUBE_CONTROLLERS=ON ^
    "-DMODERNGEKKO_WINDOWS_ICON=%CD%\release\skies.ico" || exit /b 1
)
if "%1"=="build" (
  cmake --build %B% --target moderngekko-run moderngekko-launcher || exit /b 1
  copy /y release\launcher-background.png %B%\launcher-background.png >nul || exit /b 1
  copy /y release\CREDITS.md %B%\CREDITS.md >nul || exit /b 1
)
if "%1"=="package" goto package
exit /b 0

rem package MODULE [VERSION]: the module beside the exes, then the players' zip
rem build\SkiesOfArcadiaLegends-VERSION-windows.zip (+ .sha256): the exes, module, Sys, launcher art, the
rem Visual C++ runtime DLLs (app-local, so a clean Windows needs no redistributable install), the
rem player README, COPYING and the third-party notices.
:package
if "%~2"=="" (echo usage: tools\build-release-win.bat package MODULE [VERSION] & exit /b 1)
set VERSION=%~3
if "%VERSION%"=="" set VERSION=v0.9.0
copy /y "%~2" %B%\gGEAE8P_recomp.dll >nul || exit /b 1
set P=build\SkiesOfArcadiaLegends-%VERSION%-windows
if exist %P% rmdir /s /q %P%
mkdir %P% || exit /b 1
for %%F in ("Skies of Arcadia Legends.exe" skies-run.exe gGEAE8P_recomp.dll launcher-background.png) do (
  copy /y "%B%\%%~F" %P%\ >nul || exit /b 1
)
xcopy /e /i /q /y %B%\Sys %P%\Sys >nul || exit /b 1
rem the launcher reads the DOL patch from patches\ beside itself (only our changes, keyed to the original DOL)
mkdir %P%\patches || exit /b 1
copy /y release\skies-widescreen.dolpatch %P%\patches\ >nul || exit /b 1
for %%F in (README.md CREDITS.md THIRD-PARTY.md) do copy /y release\%%F %P%\ >nul || exit /b 1
copy /y COPYING %P%\COPYING.txt >nul || exit /b 1
copy /y lib\ModernGekko\vendor\dolphin\Externals\licenses.md %P%\THIRD-PARTY-DOLPHIN.md >nul || exit /b 1
xcopy /e /i /q /y lib\ModernGekko\vendor\dolphin\LICENSES %P%\licenses >nul || exit /b 1
set CRT=
for /d %%D in ("%VCToolsRedistDir%x64\Microsoft.VC*.CRT") do set CRT=%%D
if not defined CRT (echo no Visual C++ runtime under "%VCToolsRedistDir%" & exit /b 1)
for %%F in (msvcp140.dll msvcp140_atomic_wait.dll vcruntime140.dll vcruntime140_1.dll) do (
  copy /y "%CRT%\%%F" %P%\ >nul || exit /b 1
)
if exist %P%.zip del %P%.zip
rem Windows' own tar (bsdtar) writes "/" separators; Compress-Archive writes "\", which other unzips mishandle
tar -a -cf %P%.zip -C build SkiesOfArcadiaLegends-%VERSION%-windows || exit /b 1
powershell -NoProfile -Command "(Get-FileHash '%P%.zip' -Algorithm SHA256).Hash.ToLower() + '  SkiesOfArcadiaLegends-%VERSION%-windows.zip' | Set-Content -NoNewline '%P%.zip.sha256'" || exit /b 1
echo %P%.zip
type %P%.zip.sha256
echo.
