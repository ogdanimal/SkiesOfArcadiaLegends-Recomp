@echo off
rem The emulator-free host (standalone\) as a release's runner (Windows; Linux and macOS:
rem tools/build-standalone-release.sh): Release, the three discs' DOL patches as its release table, the stock
rem Dawn package (webgpu_dawn.dll and DXC's dxil.dll / dxcompiler.dll are copied beside the exe by
rem aurora_copy_runtime_dlls, with libpng's DLLs). tools\build-release-win.bat package ships it as skies-run.exe (STANDALONE=DIR there).
rem usage: tools\build-standalone-release.bat [BUILD_DIR]     (default build\standalone-release)
if not defined VCINSTALLDIR (
  call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)
cd /d "%~dp0.." || exit /b 1
set "SB=%~1"
if "%SB%"=="" set SB=build\standalone-release
set "R=%CD:\=/%/release"
rem DAWN_PACKAGE=<dawn-windows-amd64.tar.gz>: Dawn with patches\dawn\ (tools/ci-dawn.sh; MSAA 2x / 8x), not the stock one
set "DAWNARG=-DSKIES_DAWN_PROVIDER=package"
if defined DAWN_PACKAGE set "DAWNARG=-DAURORA_DAWN_PACKAGE_URL=file:///%DAWN_PACKAGE:\=/%"
cmake -S standalone -B %SB% -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF "%DAWNARG%" ^
  "-DSKIES_RELEASE_DISC_IDS=GEAE8P;GEAP8P;GEAJ8P" ^
  "-DSKIES_RELEASE_DOL_PATCHES=%R%/skies-widescreen.dolpatch;%R%/skies-widescreen-eu.dolpatch;%R%/skies-widescreen-jp.dolpatch" || exit /b 1
cmake --build %SB% --target skies-standalone || exit /b 1
for %%F in (skies-standalone.exe webgpu_dawn.dll dxil.dll dxcompiler.dll) do (
  if not exist %SB%\%%F (echo %SB%\%%F is missing & exit /b 1)
)
echo %SB%\skies-standalone.exe
