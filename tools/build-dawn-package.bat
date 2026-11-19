@echo off
rem A Dawn package with our patches for Windows x64 (tools/build-dawn-package.sh does Linux, Android and macOS):
rem the install tree tarball Aurora's AURORA_DAWN_PROVIDER=package takes, built as encounter's release workflow
rem builds dawn-windows-amd64.tar.gz (dawn-ci.cmake: a shared webgpu_dawn.dll), except that DXC isn't built:
rem Dawn is compiled to load dxcompiler.dll / dxil.dll at run time (DAWN_USE_BUILT_DXC defined; dxcapi.h comes
rem from the Windows SDK) and the package carries the DXC DLLs of encounter's package for the same Dawn version.
rem
rem usage: tools\build-dawn-package.bat DAWN_SRC DXC_DIR [OUT_DIR]
rem   DAWN_SRC  a tree at AURORA_DAWN_REF with patches\dawn\*.patch applied and its dependencies fetched (checked)
rem   DXC_DIR   a folder holding dxcompiler.dll and dxil.dll from encounter's package (a standalone build dir
rem             made with the prebuilt Dawn has them)
rem   OUT_DIR   default build\dawn-package
rem Then: cmake ... -DSKIES_DAWN_PROVIDER=package -DAURORA_DAWN_PACKAGE_URL=file:///<OUT>/dawn-windows-amd64.tar.gz
rem SPDX-License-Identifier: GPL-3.0-or-later
setlocal
if "%~2"=="" (echo usage: tools\build-dawn-package.bat DAWN_SRC DXC_DIR [OUT_DIR] & exit /b 1)
set "SRC=%~f1"
set "DXC=%~f2"
set "OUT=%~f3"
if "%OUT%"=="" set "OUT=%~dp0..\build\dawn-package"
if not exist "%OUT%" mkdir "%OUT%"
findstr /c:"#define DAWN_NATIVE_EXTRA_SAMPLE_COUNTS" "%SRC%\include\dawn\native\DawnNative.h" >nul ^
  || (echo %SRC% lacks patches\dawn\0001 & exit /b 1)
findstr /c:"A device without VK_KHR_swapchain can't present" "%SRC%\src\dawn\native\vulkan\PhysicalDeviceVk.cpp" >nul ^
  || (echo %SRC% lacks patches\dawn\0002 & exit /b 1)
if not exist "%DXC%\dxcompiler.dll" (echo %DXC% has no dxcompiler.dll & exit /b 1)
if not exist "%DXC%\dxil.dll" (echo %DXC% has no dxil.dll & exit /b 1)
rem MSVC: the CI job sets it up (ilammy/msvc-dev-cmd); locally the Build Tools' vcvars
if not defined VCINSTALLDIR (
  call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
)
set "BUILD=%OUT%\build-dawn-windows-amd64"
set "INSTALL=%OUT%\install-dawn-windows-amd64"
rem The -D after -C override dawn-ci.cmake's DAWN_USE_BUILT_DXC=ON
cmake -S "%SRC%" -B "%BUILD%" -G Ninja -C "%SRC%\.github\workflows\dawn-ci.cmake" -DCMAKE_BUILD_TYPE=Release ^
  -DDAWN_USE_BUILT_DXC=OFF -DDAWN_FETCH_DEPENDENCIES=OFF -DDAWN_BUILD_PROTOBUF=OFF ^
  "-DCMAKE_CXX_FLAGS=/DWIN32 /D_WINDOWS /EHsc /DDAWN_USE_BUILT_DXC=1" || exit /b 1
cmake --build "%BUILD%" --config Release || exit /b 1
if exist "%INSTALL%" rmdir /s /q "%INSTALL%"
cmake --install "%BUILD%" --config Release --prefix "%INSTALL%" || exit /b 1
copy /y "%DXC%\dxcompiler.dll" "%INSTALL%\bin\" >nul || exit /b 1
copy /y "%DXC%\dxil.dll" "%INSTALL%\bin\" >nul || exit /b 1
findstr /c:"DAWN_NATIVE_EXTRA_SAMPLE_COUNTS" "%INSTALL%\include\dawn\native\DawnNative.h" >nul ^
  || (echo the installed headers lack DAWN_NATIVE_EXTRA_SAMPLE_COUNTS & exit /b 1)
rem Windows' own tar (bsdtar): from Git Bash (tools/ci-dawn.sh) GNU tar comes first and reads D:\ as a host
"%SystemRoot%\System32\tar.exe" -czf "%OUT%\dawn-windows-amd64.tar.gz" -C "%INSTALL%" . || exit /b 1
echo %OUT%\dawn-windows-amd64.tar.gz
