# Building Guide

How to build Skies of Arcadia Legends: Recompiled from source. Players don't need any of this: the
[Releases](../../releases) page has ready builds (see the [README](README.md)).

You need your own **USA** disc (`GEAE8P`). Nothing from the game is in this repository; the build
reads `main.dol` from your disc, applies the port's game patch to it
([`release/skies-widescreen.dolpatch`](release/skies-widescreen.dolpatch)) and recompiles the
result.

The pipeline, in short:

1. **DolRecomp** ([`lib/ModernGekko/vendor/dolphin/DolRecomp`](https://github.com/ExpansionPak/DolRecomp))
   translates the patched `main.dol` into native objects (LLVM backend, with a PGO profile) or C.
2. The **module template** links them into the game module (`gGEAE8P_recomp.dll` / `.so`).
3. **ModernGekko** ([`lib/ModernGekko`](https://github.com/ExpansionPak/ModernGekko)) is the runtime
   and launcher: Dolphin's hardware code (video, audio, DSP, input, memory cards) around the module.

All three come in as submodules pinned to upstream commits, and the port's changes to them live in
[`patches/`](patches/README.md).

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

### Android

The Android SDK with NDK 29.0.14206865, platform 36 and build-tools 36.0.0 (`sdkmanager`), plus
JDK 17.

## 3. Generate the code

The game patch is applied to your disc's original `main.dol` (SHA-256 `87c240ac...`), giving the
patched DOL the launcher also makes on import:

```sh
python3 -I tools/applydolpatch.py /path/to/original/main.dol release/skies-widescreen.dolpatch main.dol
```

`tools/ci-codegen.sh` is the release recipe: from a folder holding `main.dol` (the original) and the
PGO profiles `pgo/windows.profdata` (desktop, used for both x86_64 targets) and `pgo/android.profdata`,
it patches the DOL, runs DolRecomp for each target and precomputes each module's tables, so the
module builds below never need the DOL:

```sh
cmake -S lib/ModernGekko/vendor/dolphin/DolRecomp -B build/dolrecomp -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DDOLRECOMP_ENABLE_LLVM=ON -DLLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm
cmake --build build/dolrecomp --target dolrecomp
DOLRECOMP=build/dolrecomp/dolrecomp TARGETS="linux" tools/ci-codegen.sh SECRETS_DIR codegen
```

`TARGETS` takes any of `windows linux android` (default: all three). The profiles are trained with
`tools/pgo-wsl.sh` (desktop) and on the device (Android); see
[development notes, *Performance*](docs/development-notes.md#performance).

## 4. Build for your platform

### Linux

```sh
MODULE_TABLES_INC=codegen/linux/generated/module_tables.inc \
  tools/module-linux.sh codegen/linux/generated build/module-linux
tools/build-release-linux.sh configure
tools/build-release-linux.sh build
tools/build-release-linux.sh package build/module-linux/gGEAE8P_recomp.so v0.9.0
```

This makes `build/SkiesOfArcadiaLegends-v0.9.0-linux.tar.gz`: the launcher `SkiesOfArcadiaLegends`,
the runner `skies-run`, the module and Dolphin's `Sys` folder.

### Windows

From a Developer Command Prompt (or with `vcvars64.bat` found by the scripts):

```bat
set MODULE_TABLES_INC=%CD%\codegen\windows\generated\module_tables.inc
tools\module-win.bat %CD%\codegen\windows\generated %CD%\build\module
tools\build-release-win.bat configure
tools\build-release-win.bat build
tools\build-release-win.bat package build\module\gGEAE8P_recomp.dll v0.9.0
```

This makes `build\SkiesOfArcadiaLegends-v0.9.0-windows.zip` (`Skies of Arcadia Legends.exe`,
`skies-run.exe`, the module, `Sys`, the app-local Visual C++ runtime).

### Android

```sh
BRAND=1 NAME=dev PREGEN=codegen/android NDK=$ANDROID_HOME/ndk/29.0.14206865 tools/android-ws-build.sh
```

This makes a debug-signed branded APK in `build/android/apk/`. `RELEASE=1 VERSION=x.y.z` builds a
minified APK signed with the release key instead (`tools/make-release-key.sh`; the script refuses an
APK not signed with it).

### Development builds

For working on the port rather than packaging it, the `Makefile` drives the generic ModernGekko
pipeline (extract the disc, recompile, run):

```sh
make llvm-run ISO=/path/to/disc.iso      # first time
make llvm-run GAME=SkiesOfArcadiaLegends # afterwards
```

`make help` lists the targets and variables (`BACKEND=c|llvm`, `TOOLCHAIN=auto|clang|gcc|msvc`,
`LLVM_DIR`, `RUN_ARGS`, `JOBS`). The development notes cover running with keyboard profiles
(`config/`), savestates, scripted test runs (`tools/drive.py`), PGO training and the per-platform
helpers in `tools/`:

| | |
|---|---|
| [`docs/development-notes.md`](docs/development-notes.md) | how it all works, measurements, known issues; *The player build* lists every control and setting |
| [`patches/README.md`](patches/README.md) | the patches to ModernGekko, RecompCore and DolRecomp |
| [`patches/game/README.md`](patches/game/README.md) | the game patch built into the DOL (widescreen, camera, options) |
| [`docs/handoffs/`](docs/handoffs/) | status and history per area: release, code review, settings, desktop, Android |

## 5. CI and releases

GitHub Actions builds every platform the same way:

- **`validate.yml`** runs on pushes to `dev` (and by hand; `main` only ever moves to a `dev` commit it
  passed). Its `codegen` job is the only one that sees the game: it clones the private secrets repo
  (the original `main.dol` and the PGO profiles) with a read-only token, runs `tools/ci-codegen.sh`,
  deletes the inputs, and passes the generated code to the build jobs **encrypted** and for one day
  only. Codegen takes about two hours, so its encrypted output is also kept in the Actions cache,
  keyed on what it depends on (the DOL patch, DolRecomp and its patches, the table generator, LLVM,
  the secrets repo's commit, the artifact key); a push that changes none of those reuses it. The `windows`, `linux` and
  `android` jobs build and upload a package each.
- **`release.yml`** runs on a `v*` tag on a commit on `main`: it calls `validate.yml` in release mode
  (version from the tag, a signed Android APK), refuses any package that holds a disc image, a DOL or
  generated code, and publishes a GitHub Release with the three packages and `SHA256SUMS.txt`. A tag
  with a suffix (`v0.9.0-beta`) is a prerelease.

Neither runs on pull requests, so code from forks never reaches the secrets.

Repository secrets:

| Secret | What |
|---|---|
| `SOAL_SECRETS_REPO_WITH_PAT` | `https://x-access-token:<PAT>@github.com/<owner>/<secrets repo>.git`: `main.dol` (original, SHA-256 `87c240ac...`) and `pgo/windows.profdata`, `pgo/android.profdata` at its root |
| `SOAL_ARTIFACT_KEY` | any long random string (encrypts the codegen artifact) |
| `RELEASE_KEYSTORE_BASE64`, `RELEASE_STORE_PASSWORD`, `RELEASE_KEY_PASSWORD`, `RELEASE_KEY_ALIAS` | Android release signing (`tools/make-release-key.sh`, then `tools/set-release-secrets.sh`) |

**Cutting a release:** work lands on `dev`; once `validate` is green there, `tools/release.sh VERSION
SHA NOTES.md` fast-forwards `main` to that commit, tags it, waits for `release.yml` to publish and
replaces the generated text with the release notes. Release notes say what this release adds or
fixes, and link the README for everything else.
