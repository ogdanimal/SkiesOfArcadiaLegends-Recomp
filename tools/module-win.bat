@echo off
rem Link a module from an existing DolRecomp output with the module template and the clang 20 tarball,
rem configured as moderngekko-port configures it. For outputs the port doesn't build itself: LLVM objects
rem cross-generated on WSL (tools/module-llvm-win.sh) and the PGO-use step after tools/pgo-win.sh.
rem usage: tools\module-win.bat GENERATED_DIR BUILD_DIR [PROFDATA]   (absolute Windows paths)
rem Local Build Tools unless the environment already has MSVC set up (CI: msvc-dev-cmd)
if not defined VCINSTALLDIR (
  call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)
cd /d "%~dp0.." || exit /b 1
if "%LLVM_ROOT%"=="" set LLVM_ROOT=C:\dev\clang+llvm-20.1.8-x86_64-pc-windows-msvc
set "PATH=%LLVM_ROOT%\bin;%PATH%"
set CMAKE_NINJA_FORCE_RESPONSE_FILE=1
set PGO=
rem MODULE_TABLES_INC (CI): the module tables made where the DOL is, so no DOL here
set TABLES=
if not "%MODULE_TABLES_INC%"=="" set "TABLES=-DMODULE_TABLES_INC=%MODULE_TABLES_INC%"
rem Forward slashes: CMake strips the backslashes of a path inside CMAKE_C_FLAGS.
set PROF=%~3
if not "%~3"=="" set PROF=%PROF:\=/%
if not "%~3"=="" set PGO="-DCMAKE_C_FLAGS=-fprofile-use=%PROF% -Wno-profile-instr-unprofiled" "-DCMAKE_SHARED_LINKER_FLAGS=-fprofile-use=%PROF%"
cmake -S lib\ModernGekko\vendor\dolphin\module-template -B %2 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DGAME_ID=GEAE8P -DGENERATED_DIR=%1 -DGXRUNTIME_DIR=%CD%\lib\ModernGekko\vendor\dolphin\GXRuntime -DCHASSIS_ABI_DIR=%CD%\lib\ModernGekko\vendor\dolphin\Source\Core\Core\PowerPC\StaticRecomp %PGO% %TABLES% || exit /b 1
cmake --build %2 || exit /b 1
