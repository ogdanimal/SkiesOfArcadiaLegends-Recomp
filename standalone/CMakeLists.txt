cmake_minimum_required(VERSION 3.20)
project(skies_standalone C CXX)

# The emulator-free host (docs/handoffs/HANDOFF-emulator-free.md): the recompiled Skies module
# (the same g<GAMEID>_recomp shared library the Dolphin build loads) runs on this host's own
# hardware models, with GXRuntime's Aurora fork drawing the GX command stream.
#
#   cmake -S standalone -B build/standalone -G Ninja -DCMAKE_BUILD_TYPE=Release
#   (-DSKIES_STANDALONE_AURORA=OFF: headless, no window, no GPU)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

option(SKIES_STANDALONE_AURORA "Draw through GXRuntime's Aurora renderer" ON)

set(DOLPHIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/ModernGekko/vendor/dolphin")
set(GXRUNTIME_DIR "${DOLPHIN_DIR}/GXRuntime" CACHE PATH "Vendored GXRuntime (patched submodule)")
set(MODULE_ABI_DIR "${DOLPHIN_DIR}/Source/Core/Core/PowerPC/StaticRecomp")
set(MODERNGEKKO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/ModernGekko")
# A release build: the games it runs, one .dolpatch per disc ID in the same order (as the Dolphin
# build's MODERNGEKKO_REQUIRED_DISC_ID / MODERNGEKKO_DOL_PATCH_MANIFEST; each patch's output-sha256
# is the DOL the release pins). The patches ship in the package's patches/ folder.
set(SKIES_RELEASE_DISC_IDS "" CACHE STRING "Release disc IDs (GEAE8P;GEAP8P;GEAJ8P), empty: any game")
set(SKIES_RELEASE_DOL_PATCHES "" CACHE STRING "The release's .dolpatch files, in SKIES_RELEASE_DISC_IDS' order")

if(SKIES_STANDALONE_AURORA)
  # As ModernGekko builds its Aurora video backend: no gxcore/recomp frontend, no Aurora card or
  # ImGui, vendored static SDL3, prebuilt Dawn.
  set(GXRUNTIME_ENABLE_AURORA ON CACHE BOOL "" FORCE)
  set(GXRUNTIME_ENABLE_AURORA_RECOMP OFF CACHE BOOL "" FORCE)
  set(AURORA_ENABLE_IMGUI OFF CACHE BOOL "" FORCE)
  set(AURORA_ENABLE_CARD OFF CACHE BOOL "" FORCE)
  set(AURORA_ENABLE_SDL ON CACHE BOOL "" FORCE)
  set(AURORA_SDL3_PROVIDER "vendor" CACHE STRING "" FORCE)
  # Static on Android too: Aurora takes SDL's activity mutex (Android_LockActivityMutex), which libSDL3.so
  # doesn't export; SDLActivity then loads libmain.so alone (android/)
  set(AURORA_SDL3_LINKAGE "static" CACHE STRING "" FORCE)
  # Dawn: the prebuilt package, or "vendor" to build it from FETCHCONTENT_SOURCE_DIR_DAWN (a Dawn with
  # the extra MSAA sample counts patch, ~/projects/skies-aurora/dawn-src)
  set(SKIES_DAWN_PROVIDER "package" CACHE STRING "Aurora's Dawn provider (package or vendor)")
  set(AURORA_DAWN_PROVIDER "${SKIES_DAWN_PROVIDER}" CACHE STRING "" FORCE)
else()
  set(GXRUNTIME_ENABLE_AURORA OFF CACHE BOOL "" FORCE)
  set(GXRUNTIME_ENABLE_AURORA_RECOMP OFF CACHE BOOL "" FORCE)
endif()
# Aurora's libpng is shared unless BUILD_SHARED_LIBS is defined. Windows ships the DLL
# beside the exe and Android libpng16.so in the APK; on macOS and desktop Linux it is static, so the program
# needs no libpng of its own (a shared one was linked with an rpath into the build folder)
set(_skies_static_png FALSE)
if(APPLE OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID))
  set(_skies_static_png TRUE)
endif()
if(_skies_static_png)
  set(_skies_shared_defined FALSE)
  if(DEFINED BUILD_SHARED_LIBS)
    set(_skies_shared_defined TRUE)
  endif()
  set(_skies_shared ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF)
endif()
add_subdirectory("${GXRUNTIME_DIR}" GXRuntime EXCLUDE_FROM_ALL)
if(_skies_static_png)
  if(_skies_shared_defined)
    set(BUILD_SHARED_LIBS ${_skies_shared})
  else()
    unset(BUILD_SHARED_LIBS)
  endif()
endif()
# zstd for save states: Aurora's copy when it is built, else Dolphin's (Externals/zstd)
if(NOT TARGET zstd::libzstd)
  set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  add_subdirectory("${DOLPHIN_DIR}/Externals/zstd/zstd/build/cmake" zstd EXCLUDE_FROM_ALL)
  add_library(zstd::libzstd ALIAS libzstd_static)
endif()

set(SKIES_STANDALONE_SOURCES
  src/main.cpp
  src/cpu.cpp
  src/scheduler.cpp
  src/boot.cpp
  src/disc.cpp
  src/mmio.cpp
  src/hw_pi.cpp
  src/hw_vi.cpp
  src/hw_gpu.cpp
  src/dff.cpp
  src/hw_dsp.cpp
  src/hw_di.cpp
  src/hw_si.cpp
  src/hw_exi.cpp
  src/hw_memcard.cpp
  src/hw_ai.cpp
  src/video.cpp
  src/settings.cpp
  src/menu.cpp
  src/input.cpp
  src/interp60.cpp
  src/state.cpp
  src/dsphle/dsp_hle.cpp
  src/dsphle/mail_handler.cpp
  src/dsphle/ucodes.cpp
  src/dsphle/rom.cpp
  src/dsphle/init.cpp
  src/dsphle/ax.cpp
  src/dsphle/accelerator.cpp
  src/dsphle/from_dolphin.cpp
  src/interp/interp.cpp
  "${DOLPHIN_DIR}/DolRecomp/src/frontend/decoder.c"
  # The Dolphin build's save states (dolphin_state.cpp): the stream reader, its LZ4 container
  src/dolphin_state.cpp
  src/dolphin_state/dolphin_state_reader.cpp
  src/dolphin_state/sav_unpack.cpp
  "${DOLPHIN_DIR}/Externals/lz4/lz4/lib/lz4.c"
  # The release runner's part (frontend.cpp) on the runner's own code: the data folder, config.ini,
  # the release's DOL patches and pins, the states' folders
  src/frontend.cpp
  "${MODERNGEKKO_DIR}/tools/user_directory.cpp"
  "${MODERNGEKKO_DIR}/tools/dol_patch.cpp"
  "${MODERNGEKKO_DIR}/tools/frontend_config.cpp"
)
if(ANDROID)
  # The Android shell's native side: SDL's Java glue calls SDL_main in libskies.so (not libmain.so: the
  # app's launcher loads Dolphin's libmain.so for its imports, in another process)
  add_library(skies-standalone SHARED ${SKIES_STANDALONE_SOURCES})
  set_target_properties(skies-standalone PROPERTIES OUTPUT_NAME skies)
  target_link_libraries(skies-standalone PRIVATE android log)
else()
  add_executable(skies-standalone ${SKIES_STANDALONE_SOURCES})
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Dawn's prebuilt wants glibc 2.38's C23 entry points (src/glibc_compat.c on an older glibc) and GCC 13's
    # libstdc++: SKIES_STATIC_LIBSTDCXX links that in, so a release built on 22.04 needs only the system's glibc
    target_sources(skies-standalone PRIVATE src/glibc_compat.c)
    option(SKIES_STATIC_LIBSTDCXX "Linux: link libstdc++ and libgcc statically (release builds)" OFF)
    if(SKIES_STATIC_LIBSTDCXX)
      target_link_options(skies-standalone PRIVATE -static-libstdc++ -static-libgcc)
    endif()
  endif()
  # Windows: a GUI program, so no console window opens beside the game (as the release runner since
  # 2026-10-13), entered at main() all the same. Its log goes where the parent redirects stdout / stderr, else
  # to USER/Logs/skies-standalone.log (main.cpp). SKIES_STANDALONE_CONSOLE=ON keeps the console (debugging).
  option(SKIES_STANDALONE_CONSOLE "Windows: build the standalone as a console program" OFF)
  if(WIN32 AND NOT SKIES_STANDALONE_CONSOLE)
    set_target_properties(skies-standalone PROPERTIES WIN32_EXECUTABLE ON)
    if(MSVC)
      target_link_options(skies-standalone PRIVATE /ENTRY:mainCRTStartup)
    else()
      target_link_options(skies-standalone PRIVATE -Wl,/ENTRY:mainCRTStartup)
    endif()
  endif()
endif()
# release_games.inc, as ModernGekko's CMakeLists.txt writes it for the runner
set(_skies_release_games "")
list(LENGTH SKIES_RELEASE_DISC_IDS _skies_game_count)
list(LENGTH SKIES_RELEASE_DOL_PATCHES _skies_patch_count)
if(_skies_game_count AND NOT _skies_game_count EQUAL _skies_patch_count)
  message(FATAL_ERROR "SKIES_RELEASE_DOL_PATCHES needs one patch per SKIES_RELEASE_DISC_IDS entry")
endif()
foreach(_skies_index RANGE ${_skies_game_count})
  if(_skies_index EQUAL _skies_game_count)
    break()
  endif()
  list(GET SKIES_RELEASE_DISC_IDS ${_skies_index} _skies_id)
  list(GET SKIES_RELEASE_DOL_PATCHES ${_skies_index} _skies_patch)
  file(STRINGS "${_skies_patch}" _skies_sha REGEX "^output-sha256 ")
  string(REGEX REPLACE "^output-sha256 +([0-9a-fA-F]+).*" "\\1" _skies_sha "${_skies_sha}")
  get_filename_component(_skies_patch_name "${_skies_patch}" NAME)
  string(APPEND _skies_release_games "{\"${_skies_id}\", \"${_skies_sha}\", \"patches/${_skies_patch_name}\"},\n")
endforeach()
# The runner's files as they are (its own warning flags are fewer)
set_source_files_properties(
  "${MODERNGEKKO_DIR}/tools/user_directory.cpp" "${MODERNGEKKO_DIR}/tools/dol_patch.cpp"
  "${MODERNGEKKO_DIR}/tools/frontend_config.cpp"
  PROPERTIES COMPILE_OPTIONS "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-w>")
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/release_games/release_games.inc"
     CONTENT "${_skies_release_games}" @ONLY)
if(_skies_game_count)
  target_compile_definitions(skies-standalone PRIVATE MODERNGEKKO_RELEASE_GAMES=1)
endif()
target_include_directories(skies-standalone PRIVATE
  src
  "${CMAKE_CURRENT_BINARY_DIR}/release_games"
  "${DOLPHIN_DIR}/Externals/lz4/lz4/lib"
  "${MODERNGEKKO_DIR}/tools"
  "${MODERNGEKKO_DIR}/include"
  "${GXRUNTIME_DIR}/include"
  "${MODULE_ABI_DIR}"
  "${DOLPHIN_DIR}/DolRecomp/src/frontend")
# GXRuntime's CPU core: the bit-exact FP / paired-single helpers the fallback interpreter calls
target_link_libraries(skies-standalone PRIVATE GXRuntime::runtime zstd::libzstd ${CMAKE_DL_LIBS})
# The AX resampler's DSP ROM coefficients (Dolphin ships them; not game data)
target_compile_definitions(skies-standalone PRIVATE
  SKIES_DSP_COEF_DEFAULT="${DOLPHIN_DIR}/Data/Sys/GC/dsp_coef.bin")
if(SKIES_STANDALONE_AURORA)
  # Dear ImGui (Dolphin's copy) for the in-game menu, drawn through Aurora's overlay
  set(IMGUI_DIR "${DOLPHIN_DIR}/Externals/imgui/imgui")
  target_sources(skies-standalone PRIVATE
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp")
  set_source_files_properties(
    "${IMGUI_DIR}/imgui.cpp" "${IMGUI_DIR}/imgui_draw.cpp" "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    PROPERTIES COMPILE_DEFINITIONS IMGUI_DISABLE_DEMO_WINDOWS COMPILE_OPTIONS "-w")
  target_include_directories(skies-standalone PRIVATE "${IMGUI_DIR}")
  target_link_libraries(skies-standalone PRIVATE GXRuntime::aurora)
  target_compile_definitions(skies-standalone PRIVATE SKIES_STANDALONE_AURORA=1)

  # The HD texture pack import (texture_pack.cpp, the Dolphin build's TexturePackImport.cpp): .zip
  # packs through Dolphin's minizip-ng on Aurora's zlib, .7z packs through the LZMA SDK's decoder
  # (public domain; the repo's lib/lzma-sdk), as VideoCommon/CMakeLists.txt builds them there.
  target_sources(skies-standalone PRIVATE src/texture_pack.cpp src/hd_textures.cpp)
  # Controller 1's bindings in the Dolphin build's GCPadNew.ini (input.cpp): its expressions and key
  # names, dev's Core/SkiesBindings.cpp as it is
  target_sources(skies-standalone PRIVATE src/skies_bindings/Core/SkiesBindings.cpp)
  target_include_directories(skies-standalone PRIVATE src/skies_bindings)
  set(MZ_COMPAT OFF CACHE BOOL "" FORCE)
  set(MZ_ZLIB ON CACHE BOOL "" FORCE)
  foreach(_mz_off IN ITEMS MZ_BZIP2 MZ_LZMA MZ_ZSTD MZ_LIBCOMP MZ_FETCH_LIBS MZ_PKCRYPT MZ_WZAES
                           MZ_OPENSSL MZ_LIBBSD MZ_ICONV MZ_BUILD_TESTS MZ_BUILD_UNIT_TESTS
                           MZ_BUILD_FUZZ_TESTS MZ_CODE_COVERAGE)
    set(${_mz_off} OFF CACHE BOOL "" FORCE)
  endforeach()
  set(SKIP_INSTALL_ALL ON)
  set(CMAKE_DISABLE_FIND_PACKAGE_ZLIBNG TRUE)
  set(CMAKE_DISABLE_FIND_PACKAGE_ZLIB TRUE)
  set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE)
  set(ZLIB_FOUND TRUE)
  set(ZLIB_LIBRARIES ZLIB::ZLIB)
  set(ZLIB_INCLUDE_DIRS "$<TARGET_PROPERTY:ZLIB::ZLIB,INTERFACE_INCLUDE_DIRECTORIES>")
  add_subdirectory("${DOLPHIN_DIR}/Externals/minizip-ng/minizip-ng" minizip-ng EXCLUDE_FROM_ALL)
  if(MSVC)
    target_compile_options(minizip-ng PRIVATE /W0)
  else()
    target_compile_options(minizip-ng PRIVATE -w)
  endif()
  target_link_libraries(skies-standalone PRIVATE MINIZIP::minizip-ng fmt::fmt)
  set(SKIES_LZMA_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/lzma-sdk" CACHE PATH
      "LZMA SDK source (its C/ directory has 7zArcIn.c)")
  if(EXISTS "${SKIES_LZMA_SDK_DIR}/C/7zArcIn.c")
    set(_skies_7z_sources)
    foreach(_skies_7z_file IN ITEMS 7zAlloc 7zArcIn 7zBuf 7zBuf2 7zCrc 7zCrcOpt 7zDec 7zStream Bcj2
                                    Bra Bra86 BraIA64 CpuArch Delta Lzma2Dec LzmaDec Ppmd7 Ppmd7Dec)
      list(APPEND _skies_7z_sources "${SKIES_LZMA_SDK_DIR}/C/${_skies_7z_file}.c")
    endforeach()
    add_library(skies7z STATIC ${_skies_7z_sources})
    target_compile_definitions(skies7z PRIVATE Z7_PPMD_SUPPORT Z7_EXTRACT_ONLY)
    target_include_directories(skies7z PUBLIC "${SKIES_LZMA_SDK_DIR}/C")
    # Its own symbol names (src/Skies7zPrefix.h, the Dolphin build's): libchdr links another copy
    # of the SDK's LZMA decoder (the Dreamcast import below)
    set(_skies_7z_prefix "${CMAKE_CURRENT_SOURCE_DIR}/src/Skies7zPrefix.h")
    if(MSVC)
      target_compile_options(skies7z PRIVATE /W0 "/FI${_skies_7z_prefix}")
    else()
      target_compile_options(skies7z PRIVATE -w "SHELL:-include \"${_skies_7z_prefix}\"")
    endif()
    target_link_libraries(skies-standalone PRIVATE skies7z)
    target_compile_definitions(skies-standalone PRIVATE HAVE_LZMA_SDK=1)
  else()
    message(STATUS "No LZMA SDK in ${SKIES_LZMA_SDK_DIR}: the texture pack import reads .zip files and folders only")
  endif()

  # The Dreamcast features (feature-gap item 7): the Dolphin build's modules as they are on dev
  # (src/dreamcast/), on the Dolphin headers they include (src/dolphin_shim/, dreamcast_host.h,
  # dreamcast.cpp), with Highly Theoretical's AICA + ARM7 emulation (GPL-3; the repo's
  # lib/HighlyTheoretical) and libchdr for CHD images (BSD-3; lib/libchdr), built as the Dolphin
  # build's AudioCommon/CMakeLists.txt builds them.
  set(SKIES_HT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../lib/HighlyTheoretical" CACHE PATH
      "Highly Theoretical source (its Core/ directory has dcsound.c)")
  add_library(dcsound STATIC
    "${SKIES_HT_DIR}/Core/dcsound.c"
    "${SKIES_HT_DIR}/Core/arm.c"
    "${SKIES_HT_DIR}/Core/yam.c")
  target_compile_definitions(dcsound PRIVATE EMU_COMPILE EMU_LITTLE_ENDIAN HAVE_STDINT_H DISABLE_SSF)
  # yam.c's __fastcall is regparm(3) off Windows, which only x86 compilers take
  if(NOT WIN32 AND NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86|x86_64|AMD64|amd64)$")
    target_compile_options(dcsound PRIVATE "-Dregparm(x)=")
  endif()
  target_include_directories(dcsound PUBLIC "${SKIES_HT_DIR}/Core")
  if(MSVC)
    target_compile_options(dcsound PRIVATE /W0 /WX-)
  else()
    target_compile_options(dcsound PRIVATE -w)
  endif()
  # libchdr on the standalone's zstd, as a "system" one (cmake/Findzstd.cmake): Aurora's zstd also
  # builds the zstd program, so libchdr's own copy would take the name of an executable
  if(NOT TARGET zstd::zstd)
    add_library(zstd::zstd INTERFACE IMPORTED)
    set_property(TARGET zstd::zstd PROPERTY INTERFACE_LINK_LIBRARIES zstd::libzstd)
  endif()
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
  set(WITH_SYSTEM_ZSTD ON CACHE BOOL "" FORCE)
  set(CHDR_WANT_TESTS OFF CACHE BOOL "" FORCE)
  set(INSTALL_STATIC_LIBS OFF CACHE BOOL "" FORCE)
  set(WITH_LZMA_ASM OFF CACHE BOOL "" FORCE)
  set(_skies_shared ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF)
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../lib/libchdr" libchdr EXCLUDE_FROM_ALL)
  set(BUILD_SHARED_LIBS ${_skies_shared})
  foreach(_skies_chdr_target IN ITEMS chdr-static chdr-lzma miniz)
    if(TARGET ${_skies_chdr_target})
      if(MSVC)
        target_compile_options(${_skies_chdr_target} PRIVATE /W0 /WX-)
      else()
        target_compile_options(${_skies_chdr_target} PRIVATE -w)
      endif()
    endif()
  endforeach()
  add_library(skies-dreamcast STATIC
    src/dreamcast/DiscIO/SkiesUncensor.cpp
    src/dreamcast/DiscIO/SkiesUncensorConvert.cpp
    src/dreamcast/DiscIO/SkiesStreams.cpp
    src/dreamcast/AudioCommon/DCMusicImport.cpp
    src/dreamcast/AudioCommon/DCMusic.cpp
    src/dreamcast/Core/SkiesMusic.cpp
    "${DOLPHIN_DIR}/Externals/picojson/picojson.cpp")
  target_include_directories(skies-dreamcast PRIVATE
    src/dreamcast src/dolphin_shim src "${DOLPHIN_DIR}/Externals/picojson")
  target_compile_definitions(skies-dreamcast PRIVATE HAVE_DCMUSIC=1 HAVE_LIBCHDR=1)
  target_link_libraries(skies-dreamcast PRIVATE dcsound chdr-static fmt::fmt)
  target_sources(skies-standalone PRIVATE src/dreamcast.cpp)
  set_source_files_properties(src/dreamcast.cpp PROPERTIES INCLUDE_DIRECTORIES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/dreamcast;${CMAKE_CURRENT_SOURCE_DIR}/src/dolphin_shim")
  # The Sys folder with Skies/uncensor-manifest.json (names and hashes, no game data): --sys DIR
  # overrides it (the Android app is given a copy)
  target_compile_definitions(skies-standalone PRIVATE
    SKIES_SYS_DEFAULT="${CMAKE_CURRENT_SOURCE_DIR}/data/Sys")
  target_link_libraries(skies-standalone PRIVATE skies-dreamcast)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(skies-standalone PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
