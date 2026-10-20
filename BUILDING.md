# Building Guide

How to build Skies of Arcadia Legends: Recompiled from source. Players don't need any of this: the
[Releases](../../releases) page has ready builds (see the [README](README.md)).

You need your own **USA** (`GEAE8P`), **European** (`GEAP8P`) or **Japanese** (`GEAJ8P`) disc; a
release build carries all three games' modules, so building one needs the three DOLs. Nothing from
the game is in this repository; the build reads `main.dol` from your disc, applies the port's game
patch to it ([`release/skies-widescreen.dolpatch`](release/skies-widescreen.dolpatch), European
[`release/skies-widescreen-eu.dolpatch`](release/skies-widescreen-eu.dolpatch), Japanese
[`release/skies-widescreen-jp.dolpatch`](release/skies-widescreen-jp.dolpatch)) and recompiles the
result.

The pipeline, in short:

1. **DolRecomp** ([`lib/ModernGekko/vendor/dolphin/DolRecomp`](https://github.com/ExpansionPak/DolRecomp))
   translates the patched `main.dol` into native objects (LLVM backend, with a PGO profile) or C.
2. The **module template** links them into the game module (`gGEAE8P_recomp.dll` / `.so`; the
   European one is `gGEAP8P_recomp`, the Japanese one `gGEAJ8P_recomp`).
3. **ModernGekko** ([`lib/ModernGekko`](https://github.com/ExpansionPak/ModernGekko)) is the runtime
   and launcher: Dolphin's hardware code (video, audio, DSP, input, memory cards) around the module.

All three come in as submodules pinned to upstream commits, and the port's changes to them live in
[`patches/`](patches/README.md). The libraries the port adds sit beside them in `lib/`: Highly
Theoretical and libchdr (submodules, for the Dreamcast music) and the LZMA SDK's 7z decoder
(`lib/lzma-sdk`, copied in unchanged, for `.7z` texture packs; without it the HD textures import
takes `.zip` packs and folders only).

## 1. Get the source

```sh
git clone --recurse-submodules https://github.com/ogdanimal/SkiesOfArcadiaLegends-Recomp.git
cd SkiesOfArcadiaLegends-Recomp
tools/apply-patches.sh
```

If you cloned without `--recurse-submodules`, run `git submodule update --init --recursive` first.
`tools/apply-patches.sh` applies [`patches/`](patches/README.md) to the submodules in order
(`--check` only tests that they apply). After editing a submodule,
`tools/regen-recompcore-patch.sh NAME [NEW_FILE...]` rewrites one patch from the working copy and
`tools/check-patches.sh` proves the whole set still reproduces the working copies.

> [!NOTE]
> `lib/ModernGekko` vendors a large chunk of Dolphin's dependency tree (SDL, fmt, imgui, Vulkan
> headers, etc.), so the first submodule sync takes a while and pulls a few hundred MB.

## 2. Install dependencies

CMake, Ninja and pkg-config, plus a C11/C++23 toolchain, Python 3, and **LLVM 20** for DolRecomp's
LLVM backend (the one every release uses).

### Linux

Ubuntu/Debian, matching what `lib/ModernGekko/vendor/dolphin`'s own CI installs for its NoGUI build:

```sh
sudo apt-get install -y ninja-build build-essential pkg-config cmake python3 \
  libevdev-dev libudev-dev libgtk-3-dev libsystemd-dev \
  libbluetooth-dev libasound2-dev libpulse-dev libgl1-mesa-dev libegl-dev \
  libxrandr-dev libxi-dev llvm-20-dev
```

### Windows

Visual Studio 2022 or later (or the standalone Build Tools) with "Desktop development with C++",
CMake and Ninja on `PATH`, and LLVM 20's release tarball
(`clang+llvm-20.1.8-x86_64-pc-windows-msvc.tar.xz`; `LLVM_ROOT` points at it). Dolphin's Windows
dependencies are vendored under `lib/ModernGekko/vendor/dolphin/Externals/`.

### macOS

An Apple Silicon Mac on macOS 13 or later, with the Xcode Command Line Tools
(`xcode-select --install`; the full Xcode isn't needed), CMake and Ninja (Homebrew, or
`python3 -m pip install --user cmake ninja`), and LLVM 20's release tarball
(`LLVM-20.1.8-macOS-ARM64.tar.xz`). That LLVM's CMake package asks for a static zstd; build the one
Dolphin vendors and point `CMAKE_PREFIX_PATH` at it when configuring DolRecomp:

```sh
cmake -S lib/ModernGekko/vendor/dolphin/Externals/zstd/zstd/build/cmake -B build/zstd -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DZSTD_BUILD_SHARED=OFF -DZSTD_BUILD_PROGRAMS=OFF -DZSTD_BUILD_TESTS=OFF \
  -DCMAKE_INSTALL_PREFIX=$PWD/build/zstd-install
cmake --build build/zstd && cmake --install build/zstd
```

The release draws with Metal (OpenGL as the fallback). Vulkan is left out: it would need MoltenVK,
which only builds with the full Xcode.

### Android

The Android SDK with NDK 29.0.14206865, platform 36 and build-tools 36.0.0 (`sdkmanager`), plus
JDK 17.

## 3. Generate the code

The game patch is applied to your disc's original `main.dol` (USA SHA-256 `87c240ac...`, European
`d0e9fc91...`, Japanese `4aa0f3dc...`), giving the patched DOL the launcher also makes on import:

```sh
python3 -I tools/applydolpatch.py /path/to/original/main.dol release/skies-widescreen.dolpatch main.dol
python3 -I tools/applydolpatch.py /path/to/european/main.dol release/skies-widescreen-eu.dolpatch main-eu.dol
python3 -I tools/applydolpatch.py /path/to/japanese/main.dol release/skies-widescreen-jp.dolpatch main-jp.dol
```

`tools/ci-codegen.sh` is the release recipe: from a folder holding `main.dol` (the USA original),
`main-eu.dol` (the European original), `main-jp.dol` (the Japanese original; without either, the
games it has) and the PGO profiles
`pgo/windows.profdata` (desktop, used for both x86_64 targets), `pgo/android.profdata` and optionally
`pgo/macos.profdata`, it patches each DOL, runs DolRecomp for each target in
`tools/codegen-targets.txt` (Windows, Linux, Android, macOS; one line each: the LLVM triple, CPU and
profile) and precomputes each module's tables, so the module builds below never need the DOL. The
USA game goes to `windows/`, `linux/`, `android/` and `macos/`, the European one to `windows-eu/` and
so on, the Japanese one to `windows-jp/` and so on, built without PGO unless `pgo/windows-eu.profdata`
/ `pgo/android-eu.profdata` / `pgo/macos-eu.profdata` (`-jp` likewise) exist (the profiles are keyed
by code addresses, which differ between the games). A target without its profile is built without PGO:

```sh
cmake -S lib/ModernGekko/vendor/dolphin/DolRecomp -B build/dolrecomp -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DDOLRECOMP_ENABLE_LLVM=ON -DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm
cmake --build build/dolrecomp --target dolrecomp
DOLRECOMP=build/dolrecomp/dolrecomp TARGETS="linux" tools/ci-codegen.sh SECRETS_DIR codegen
```

On macOS, configure DolRecomp with `-DLLVM_DIR=/path/to/LLVM-20.1.8-macOS-ARM64/lib/cmake/llvm
-DCMAKE_PREFIX_PATH=$PWD/build/zstd-install` and run the codegen with `TARGETS="macos"`.

`TARGETS` takes any of the targets in `tools/codegen-targets.txt` (`windows linux android macos`, the
default: all four), `REGIONS` any of `us`, `eu` and `jp`. That file has each target's LLVM triple, CPU and
PGO profile. The profiles are trained with
`tools/pgo-wsl.sh` (desktop) and on the device (Android); see
[development notes, *Performance*](docs/development-notes.md#performance).

## 4. Build for your platform

### Linux

```sh
MODULE_TABLES_INC=codegen/linux/generated/module_tables.inc \
  tools/module-linux.sh codegen/linux/generated build/module-linux
MODULE_TABLES_INC=codegen/linux-eu/generated/module_tables.inc GAME_ID=GEAP8P \
  tools/module-linux.sh codegen/linux-eu/generated build/module-linux-eu
MODULE_TABLES_INC=codegen/linux-jp/generated/module_tables.inc GAME_ID=GEAJ8P \
  tools/module-linux.sh codegen/linux-jp/generated build/module-linux-jp
REGIONS="us eu jp" tools/build-release-linux.sh configure
tools/build-release-linux.sh build
tools/build-release-linux.sh package build/module-linux/gGEAE8P_recomp.so \
  build/module-linux-eu/gGEAP8P_recomp.so build/module-linux-jp/gGEAJ8P_recomp.so v0.60.0-beta
```

This makes `build/SkiesOfArcadiaLegends-v0.60.0-beta-linux.tar.gz`: the launcher
`SkiesOfArcadiaLegends`, the runner `skies-run`, the three modules and Dolphin's `Sys` folder.
Without `REGIONS="us eu jp"` at configure the release takes the USA disc alone, and `package` needs
only the USA module (`REGIONS="us eu"`: the USA and European games; `package` tells the optional
modules apart by their names). Dolphin's libraries (bzip2, zstd, zlib and the rest) are its own bundled copies, linked
in (`USE_SYSTEM_LIBS=OFF`), so the package doesn't need the build machine's distribution.

```sh
tools/build-release-linux.sh appimage v0.60.0-beta
```

then makes `build/SkiesOfArcadiaLegends-v0.60.0-beta-x86_64.AppImage` from that folder, with
the remaining libraries from the build machine inside (a pinned linuxdeploy, downloaded to
`build/tools` and checked against its SHA-256). glibc and the graphics drivers are the player's, so
the AppImage needs a glibc at least as new as the build machine's. CI builds it in an Ubuntu 22.04
container with GCC 12 (the oldest Ubuntu LTS still supported, and Dolphin's minimum compiler), so the
release runs on glibc 2.35 and later; `BUILD_DIR=...` puts a build in another folder than
`lib/ModernGekko/build-release-linux`.

### Windows

From a Developer Command Prompt (or with `vcvars64.bat` found by the scripts):

```bat
set MODULE_TABLES_INC=%CD%\codegen\windows\generated\module_tables.inc
tools\module-win.bat %CD%\codegen\windows\generated %CD%\build\module
set MODULE_TABLES_INC=%CD%\codegen\windows-eu\generated\module_tables.inc
set GAME_ID=GEAP8P
tools\module-win.bat %CD%\codegen\windows-eu\generated %CD%\build\module-eu
set MODULE_TABLES_INC=%CD%\codegen\windows-jp\generated\module_tables.inc
set GAME_ID=GEAJ8P
tools\module-win.bat %CD%\codegen\windows-jp\generated %CD%\build\module-jp
set GAME_ID=
set REGIONS=us eu jp
tools\build-release-win.bat configure
tools\build-release-win.bat build
tools\build-release-win.bat package build\module\gGEAE8P_recomp.dll build\module-eu\gGEAP8P_recomp.dll build\module-jp\gGEAJ8P_recomp.dll v0.60.0-beta
```

This makes `build\SkiesOfArcadiaLegends-v0.60.0-beta-windows.zip` (`Skies of Arcadia Legends.exe`,
`skies-run.exe`, the three modules, `Sys`, the app-local Visual C++ runtime). `REGIONS` works as on
Linux.

### macOS

```sh
MODULE_TABLES_INC=codegen/macos/generated/module_tables.inc \
  tools/module-macos.sh codegen/macos/generated build/module-macos
MODULE_TABLES_INC=codegen/macos-eu/generated/module_tables.inc GAME_ID=GEAP8P \
  tools/module-macos.sh codegen/macos-eu/generated build/module-macos-eu
MODULE_TABLES_INC=codegen/macos-jp/generated/module_tables.inc GAME_ID=GEAJ8P \
  tools/module-macos.sh codegen/macos-jp/generated build/module-macos-jp
REGIONS="us eu jp" tools/build-release-macos.sh configure
tools/build-release-macos.sh build
tools/build-release-macos.sh package build/module-macos/gGEAE8P_recomp.dylib \
  build/module-macos-eu/gGEAP8P_recomp.dylib build/module-macos-jp/gGEAJ8P_recomp.dylib VERSION
```

This makes `build/SkiesOfArcadiaLegends-VERSION-macos.zip`: `Skies of Arcadia Legends.app` (the
launcher, `skies-run` and the modules in `Contents/MacOS`; `Sys`, the DOL patches and the launcher
background in `Contents/Resources`) and the documents. The app is signed ad hoc (Apple Silicon runs
nothing unsigned), not with a Developer ID, so Gatekeeper asks before the first start (see the
[README](README.md#getting-started)). The macOS codegen is built without PGO until there is a
`pgo/macos.profdata` (none trained yet: the desktop profile was trained on x86_64 and the Android one
on Android's target). `REGIONS` works as on Linux. CI runs these steps in validate.yml's `macos` job
(a `macos-15` runner with Xcode 26 selected, Homebrew's Ninja), from the `macos us` / `macos eu` /
`macos jp` codegen jobs' `codegen/macos{,-eu,-jp}`. The script builds Dolphin's own zstd, lz4, LZO and libusb
(`USE_SYSTEM_*=OFF`; the runner has Homebrew's), and `package` stops if the launcher or runner links
anything in `/opt/homebrew` or `/usr/local`, which a player's Mac doesn't have.

### Android

```sh
BRAND=1 NAME=dev PREGEN=codegen/android NDK=$ANDROID_HOME/ndk/29.0.14206865 tools/android-ws-build.sh
```

This makes a debug-signed branded APK in `build/android/apk/`. It has the release app's ID
(`com.skiesofarcadia.recomp`) but another signature, so it can't update a release install; `TEST_ID=1`
builds it as `com.skiesofarcadia.recomp.test` ("Skies of Arcadia Legends (test)"), which installs
beside the release app and never touches its data. `PREGEN_EU=codegen/android-eu` (or
a European patched DOL at `DOL_EU`) adds the European module and its patch, `PREGEN_JP=codegen/android-jp`
(or `DOL_JP`) the Japanese one; without them the APK takes the USA disc alone. `RELEASE=1 VERSION=x.y.z` builds a
minified APK, built unsigned by Gradle and then signed with the release key by `tools/sign-apk.sh`
(`tools/make-release-key.sh`; it refuses an APK not signed with that key). `SIGN=0` stops at the
unsigned APK, as CI does: there a separate `android-sign` job signs it, so Gradle and the build's
dependencies never run alongside the signing secrets. The build stays arm64-only through
`-Pandroid.injected.build.abi`, an Android Studio property that makes the Android Gradle plugin mark the
APK `android:testOnly` (installable only with `adb install -t`); the script also passes
`-Pandroid.injected.testOnly=false`, and `tools/sign-apk.sh` refuses a test-only APK.

### Development builds

For working on the port rather than packaging it, the `Makefile` drives the generic ModernGekko
pipeline (extract the disc, recompile, run):

```sh
make llvm-run ISO=/path/to/disc.iso      # first time
make llvm-run GAME=SkiesOfArcadiaLegends # afterwards
```

`make help` lists the targets and variables (`BACKEND=c|llvm`, `TOOLCHAIN=auto|clang|gcc|msvc`,
`LLVM_DIR`, `RUN_ARGS`, `JOBS`). The development notes cover the controller and keyboard
(the game adds a keyboard layout itself; old profiles in `config/`), savestates, scripted test runs
(`tools/drive.py`), PGO training and the per-platform
helpers in `tools/`:

| | |
|---|---|
| [`docs/development-notes.md`](docs/development-notes.md) | how it all works, measurements, known issues; *The player build* lists every control and setting |
| [`patches/README.md`](patches/README.md) | the patches to ModernGekko, RecompCore and DolRecomp |
| [`patches/game/README.md`](patches/game/README.md) | the game patch built into the DOL (widescreen, camera, options) |
| [`docs/handoffs/`](docs/handoffs/) | status and history per area: release, code review, settings, desktop, Android, macOS, input, the European disc and Dreamcast music, the Japanese disc, HD textures |

## 5. CI and releases

GitHub Actions builds every platform the same way:

- **`validate.yml`** runs on pushes to `dev` (and by hand; `main` only ever moves to a `dev` commit it
  passed), except pushes that change only Markdown, `.gitignore`, `.recomp.json` or the issue
  templates. Its `codegen` jobs are the only ones that see the game: it clones the private secrets repo
  (the original `main.dol` and the PGO profiles) with a read-only token, runs `tools/ci-codegen.sh`,
  deletes the inputs, and passes the generated code to the build jobs **encrypted**. It is a matrix:
  one job per target and region (8, at the same time; 7 to 17 minutes each when they have to
  generate, about 20 for all of them). Each one's encrypted output is kept under a key made of what it depends on (the
  region's DOL patch, the target's line in `tools/codegen-targets.txt`, DolRecomp and its patches,
  the table generator, LLVM, the secrets repo's commit, the artifact key): in the Actions cache, and
  as an artifact named by that key for 30 days. A run whose inputs are unchanged reuses it, so a new
  or changed target regenerates only its own code; the artifact is what lets a release tag (which
  can't see `dev`'s cache) reuse what validate made. The `windows`, `linux`, `macos` and `android`
  jobs build and upload a package each.
- **`release.yml`** runs on a `v*` tag on a commit on `main`: it calls `validate.yml` in release mode
  (version from the tag, a signed Android APK), refuses any package that holds a disc image, a DOL or
  generated code, and publishes a GitHub Release with the packages (the Windows zip, the Linux tar.gz
  and AppImage, the macOS app zip, the Android APK) and `SHA256SUMS.txt`. A tag
  with a suffix (`v0.60.0-beta`) is a prerelease.

Neither runs on pull requests, so code from forks never reaches the secrets.

Secrets (in the `ci` environment, and `release` for signing; none at repository level):

| Secret | What |
|---|---|
| `SOAL_SECRETS_REPO_WITH_PAT` | `https://x-access-token:<PAT>@github.com/<owner>/<secrets repo>.git`: `main.dol` (USA original, SHA-256 `87c240ac...`), `main-eu.dol` (European original, `d0e9fc91...`), `main-jp.dol` (Japanese original, `4aa0f3dc...`) and `pgo/windows.profdata`, `pgo/android.profdata` at its root (`pgo/macos.profdata` and the European and Japanese profiles `pgo/*-eu.profdata`, `pgo/*-jp.profdata` optional) |
| `SOAL_ARTIFACT_KEY` | any long random string (encrypts the codegen artifact) |
| `RELEASE_KEYSTORE_BASE64`, `RELEASE_STORE_PASSWORD`, `RELEASE_KEY_PASSWORD`, `RELEASE_KEY_ALIAS` | Android release signing (`tools/make-release-key.sh`, then `tools/set-release-secrets.sh`) |

**Cutting a release:** work lands on `dev`; once `validate` is green there, `tools/release.sh VERSION
SHA NOTES.md` fast-forwards `main` to that commit, tags it, waits for `release.yml` to publish and
replaces the generated text with the release notes. The `android-sign` job waits for the owner's
approval of the `release` environment (Actions page) before it signs. A tag on a commit validated
within the last 30 days reuses its codegen, so the release takes about 25 minutes; otherwise it
generates again (about 20 minutes more, all targets at once; before 2026-10-19 the targets ran one
after another, 1.5 to 2 hours). Release notes say what this release adds or fixes, and link the
README for everything else.
