@echo off
rem Native Windows (MSVC) build. The Makefile is Unix-only, so this runs the same CMake steps.
rem usage: tools\build-win.bat configure|build|module|module-msvc|pgo
rem module builds the game module with clang from the LLVM release tarball unpacked at LLVM_ROOT (default below)
rem into build\modules-clang; pgo builds the same module with PGO into build\modules-clang-pgo (train it with
rem tools/pgo-win.sh from WSL, which runs this). module-msvc uses cl into build\modules: same picture, but half the speed (docs/development-notes.md).
rem Sets up the VS Build Tools 18 environment itself; run it from any cmd (or via cmd.exe from WSL).
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
cd /d "%~dp0.." || exit /b 1
if "%LLVM_ROOT%"=="" set LLVM_ROOT=C:\dev\clang+llvm-20.1.8-x86_64-pc-windows-msvc
if "%1"=="configure" (
  cmake -S lib\ModernGekko -B lib\ModernGekko\build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DDOLRECOMP_ENABLE_LLVM=OFF || exit /b 1
)
if "%1"=="build" (
  cmake --build lib\ModernGekko\build --target moderngekko-port moderngekko-run || exit /b 1
)
if "%1"=="module-msvc" (
  lib\ModernGekko\build\moderngekko-port.exe build extracted\SkiesOfArcadiaLegends --backend c --toolchain msvc --output build\modules || exit /b 1
)
if "%1"=="module" (
  set "PATH=%LLVM_ROOT%\bin;%PATH%"
  lib\ModernGekko\build\moderngekko-port.exe build extracted\SkiesOfArcadiaLegends --backend c --toolchain clang --output build\modules-clang || exit /b 1
)
if "%1"=="pgo" (
  set "PATH=%LLVM_ROOT%\bin;%PATH%"
  lib\ModernGekko\build\moderngekko-port.exe pgo-run extracted\SkiesOfArcadiaLegends --backend c --toolchain clang --llvm-profdata %LLVM_ROOT%\bin\llvm-profdata.exe --output build\modules-clang-pgo --keep-work -- --user-dir build\user-bench --automation-dir build\auto --load-state build\savestates\01-opening-cutscene.sav || exit /b 1
)
