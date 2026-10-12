@echo off
rem Windows counterpart of tools/play.sh: run the module with the automation dir build\auto.
rem usage: tools\run-win.bat LOG [extra moderngekko-run args, e.g. --load-state FILE]
rem Drive it with tools/drive.py (AUTO=build/auto), also from WSL.
cd /d "%~dp0.." || exit /b 1
set LOG=%1
shift
set ARGS=
:loop
if "%1"=="" goto run
set ARGS=%ARGS% %1
shift
goto loop
:run
if not exist build\auto\commands mkdir build\auto\commands
del /q build\auto\status.txt build\auto\commands\* 2>nul
rem Default: the LLVM + PGO module (tools/module-llvm-win.sh). Set MODULE to run another, e.g. the clang
rem one: build\modules-clang-pgo\use-build\gGEAE8P_recomp.dll
if "%MODULE%"=="" set MODULE=build\modules-llvm-pgo-fix\gGEAE8P_recomp.dll
rem RUNTIME picks another runtime binary in lib\ModernGekko\build (beside Sys\); USER_DIR another user directory;
rem GAME another game root (e.g. build\game\GEAE8P-ws, from tools/gamepatch.py).
if "%RUNTIME%"=="" set RUNTIME=lib\ModernGekko\build\moderngekko-run.exe
if "%USER_DIR%"=="" set USER_DIR=build\user
if "%GAME%"=="" set GAME=extracted\SkiesOfArcadiaLegends
%RUNTIME% --game %GAME% --module %MODULE% --user-dir %USER_DIR% --automation-dir build\auto %ARGS% > %LOG% 2>&1
