# Third-party software

Skies of Arcadia Legends: Recompiled is distributed under the GNU General Public License, version 3
(`COPYING`). It includes or links the following:

- [ModernGekko](https://github.com/ExpansionPak/ModernGekko) (runtime and launcher): GPL-3.0
- [DolRecomp](https://github.com/ExpansionPak/DolRecomp) (static recompiler; its runtime support
  code is linked into the game module): GPL-3.0
- [Dolphin](https://github.com/dolphin-emu/dolphin) (video, audio, input and hardware code, through
  ModernGekko's RecompCore fork): GPL-2.0-or-later, used here under GPL-3.0
- [{fmt}](https://github.com/fmtlib/fmt): MIT
- [Highly Theoretical](https://github.com/kode54/Highly_Theoretical) by Neill Corlett, maintained by
  kode54 (the Dreamcast sound chip, AICA with its ARM7, for the Dreamcast music option): GPL-3.0
- [libchdr](https://github.com/rtissera/libchdr) by Romain Tisserand (reads CHD disc images for the
  Dreamcast music import):
  BSD-3-Clause (`licenses/libchdr-LICENSE.txt`), with its bundled LZMA SDK (public domain), miniz (MIT)
  and Zstandard decoder (BSD-3-Clause)
- The [LZMA SDK](https://www.7-zip.org/sdk.html) by Igor Pavlov, version 23.01 (its 7z decoder, which
  reads `.7z` texture packs for the HD textures import): public domain

The new engine (the game's runtime in the `v0.90.0-standalone` test builds; licence texts in `licenses/`):

- GXRuntime (the GameCube hardware layer the recompiled game runs on, part of ModernGekko's Dolphin tree): GPL-3.0
- [Aurora](https://github.com/encounter/aurora) by Luke Street (GameCube graphics on WebGPU): MIT
  (`licenses/Aurora-LICENSE.txt`)
- [Dawn](https://dawn.googlesource.com/dawn) by the Dawn & Tint Authors (WebGPU, with its Tint shader compiler):
  BSD-3-Clause (`licenses/Dawn-LICENSE.txt`), with [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools)
  and [Abseil](https://github.com/abseil/abseil-cpp): Apache-2.0 (`licenses/SPIRV-Tools-LICENSE.txt`,
  `licenses/abseil-LICENSE.txt`)
- Windows: the [DirectX Shader Compiler](https://github.com/microsoft/DirectXShaderCompiler) (`dxcompiler.dll`,
  `dxil.dll`, from Dawn's Windows package): University of Illinois/NCSA (`licenses/DirectXShaderCompiler-LICENSE.txt`)
- [SDL](https://libsdl.org/) 3: zlib (`licenses/SDL-LICENSE.txt`)
- [xxHash](https://github.com/Cyan4973/xxHash), [LZ4](https://github.com/lz4/lz4): BSD-2-Clause;
  [Zstandard](https://github.com/facebook/zstd): BSD-3-Clause; [libpng](http://www.libpng.org/pub/png/libpng.html):
  the libpng licence; [zlib](https://zlib.net/) and [minizip-ng](https://github.com/zlib-ng/minizip-ng): zlib;
  [picojson](https://github.com/kazuho/picojson): BSD-2-Clause; SQLite (Aurora's pipeline cache): public domain
  (`licenses/xxHash-LICENSE.txt`, `lz4-LICENSE.txt`, `zstd-LICENSE.txt`, `libpng-LICENSE.txt`, `zlib-LICENSE.txt`,
  `minizip-ng-LICENSE.txt`, `picojson-LICENSE.txt`)
- Android: [libadrenotools](https://github.com/bylaws/libadrenotools) by Billy Laws (custom GPU drivers):
  BSD-2-Clause (`licenses/libadrenotools-LICENSE.txt`)

Dolphin's own third-party components (SDL, Dear ImGui, zlib-ng, Zstandard, cubeb, mbed TLS and others)
are listed with their licences in `THIRD-PARTY-DOLPHIN.md`, a copy of Dolphin's `Externals/licenses.md`.

The source code for this release, including every change made to the projects above (`patches/`), is
available from the project's repository, https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp, at
this release's tag.

Fonts are not bundled: the launcher and the in-game menu use the system's own fonts (Segoe UI on
Windows, Roboto on Android).

The launcher background art (I_LIKE_SONIC) and the app icon art (Reckoner) are from SteamGridDB and
belong to their artists; they aren't covered by the GPL.
