@echo off
rem Play the current game build (DOL ws16: widescreen incl. the ship battle HUD, camera control, QOL options incl. ship battles, altitude gauge at the edge; build\game\GEAE8P-ws16
rem + its module) with the user directory build\user-ws. Picture, frame rate and the rest are set in
rem game (hold Z, press Start; NOTES "The player build"). Extra arguments go to tools\run-win.bat,
rem e.g. --load-state build\savestates\01-opening-cutscene.sav
cd /d "%~dp0.." || exit /b 1
set GAME=build\game\GEAE8P-ws16
set MODULE=build\modules-llvm-pgo-ws16\gGEAE8P_recomp.dll
set USER_DIR=build\user-ws
call tools\run-win.bat build\play-ws.log %*
