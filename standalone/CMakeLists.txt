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

if(SKIES_STANDALONE_AURORA)
  # As ModernGekko builds its Aurora video backend: no gxcore/recomp frontend, no Aurora card or
  # ImGui, vendored static SDL3, prebuilt Dawn.
  set(GXRUNTIME_ENABLE_AURORA ON CACHE BOOL "" FORCE)
  set(GXRUNTIME_ENABLE_AURORA_RECOMP OFF CACHE BOOL "" FORCE)
  set(AURORA_ENABLE_IMGUI OFF CACHE BOOL "" FORCE)
  set(AURORA_ENABLE_CARD OFF CACHE BOOL "" FORCE)
  set(AURORA_ENABLE_SDL ON CACHE BOOL "" FORCE)
  set(AURORA_SDL3_PROVIDER "vendor" CACHE STRING "" FORCE)
  set(AURORA_SDL3_LINKAGE "static" CACHE STRING "" FORCE)
  set(AURORA_DAWN_PROVIDER "package" CACHE STRING "" FORCE)
else()
  set(GXRUNTIME_ENABLE_AURORA OFF CACHE BOOL "" FORCE)
  set(GXRUNTIME_ENABLE_AURORA_RECOMP OFF CACHE BOOL "" FORCE)
endif()
add_subdirectory("${GXRUNTIME_DIR}" GXRuntime EXCLUDE_FROM_ALL)

add_executable(skies-standalone
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
  src/dsphle/dsp_hle.cpp
  src/dsphle/mail_handler.cpp
  src/dsphle/ucodes.cpp
  src/dsphle/rom.cpp
  src/dsphle/init.cpp
  src/dsphle/ax.cpp
  src/dsphle/accelerator.cpp
  src/interp/interp.cpp
  "${DOLPHIN_DIR}/DolRecomp/src/frontend/decoder.c"
)
target_include_directories(skies-standalone PRIVATE
  src
  "${GXRUNTIME_DIR}/include"
  "${MODULE_ABI_DIR}"
  "${DOLPHIN_DIR}/DolRecomp/src/frontend")
# GXRuntime's CPU core: the bit-exact FP / paired-single helpers the fallback interpreter calls
target_link_libraries(skies-standalone PRIVATE GXRuntime::runtime ${CMAKE_DL_LIBS})
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
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(skies-standalone PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
