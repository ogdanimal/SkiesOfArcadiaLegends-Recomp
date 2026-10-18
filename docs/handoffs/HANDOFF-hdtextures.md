# Handoff: HD texture packs (2026-10-17; done, merge held for the widescreen fix)

Players import an HD texture pack they downloaded; the recomp never ships one. Worked on in the
worktree `~/projects/skies-hdtex` (branch `hdtex`, from `dev` 0e3a0cf, kept rebased on `dev`); its submodules and Dolphin
Externals are git worktrees of the main checkout's, with `tools/apply-patches.sh` applied.

## The pack tested

"Skies of Arcadia HD v0.06" by Bighead and frozenwings (Dolphin forums thread "Skies of Arcadia HD
v0.06 (June 27, 2020)"). No licence or permission to redistribute: **players bring their own**, like
the disc and the Dreamcast music.

- Downloads: `SoA_HD_v0.06_DDS.7z` (BC7 + uncompressed DDS, 92 MB, unpacks to 732 MB) and
  `SoA_HD_v0.06_PNG.7z` (176 MB / 251 MB; mipmaps as separate `_mipN` files). Both: one solid LZMA2
  block, dictionary 256 MB. The "Dolphin Resource Pack" download is the same core textures for
  Dolphin's Resource Pack Manager (plus a widescreen flight meter and French text): not used.
- Layout: `SoA_HD_v0.06_*/GEA/{Characters,Environment,Misc,Special Effects,Text,UI,Weapons}` and
  `Optional/` (4:3 and widescreen flight meters, French textures, French-English restore). `GEA` =
  both discs. 745 distinct textures; one stray duplicate `Text/tex1_24x24_40710d2632c57a45_0(1).dds`.
- Coverage: the three leads' faces and costumes, ~45 skies, UI and battle menus, text drawn as
  images, magic and some enemy specials, Vyse's and Aika's first weapons, intro and save point.
  Nothing for other characters, towns, dungeons, ships.

Results on Windows (ws20 dev runner, PGO module, Vulkan, 3x; test user dir
`C:\dev\SkiesRecomp\build\user-hdtex`, shots `build\hdtex3x1080-*`):

- Works with Dolphin's custom texture loader as is (`GFX.ini [Settings] HiresTextures = True`,
  textures in `Load/Textures/GEA`). Speed 1.000 with and without it.
- 149 of the 745 used across 9 states; the same 149 with EFB copies in RAM or texture-only. The
  pack's note that "a very important UI texture requires Store EFB Copies to Texture Only" isn't
  reproduced yet (upstream `GEA.ini` has `EFBToTextureEnable = False`).
- BC7 DDS on a GPU without BPTC is skipped silently (`CustomTextureData.cpp`, `bSupportsBPTCTextures`).
  The Retroid runs the stock Qualcomm driver (V@0746.0, Adreno 650): probably no BC7 there.
- Older states `09-title`, `12-streaks` raise "Invalid read from 0x00002874, PC = 0x80001dd8" on ws20
  (a Dolphin Warning dialog that blocks the runner): use 01, 02, 03, 05, 06, 07, 10, 17, 19.

## The import (this branch)

- `VideoCommon/TexturePackImport.{h,cpp}` (recompcore patch): `Import(pack, Load/Textures/, ...)`,
  `Describe`, `Bc7Count`, `Remove`. Takes a .7z (LZMA SDK 7z decoder, `lib/lzma-sdk`, public
  domain; LZMA / LZMA2 folders decoded as a stream with a window no larger than the folder, other
  methods through the SDK in memory), a .zip (minizip-ng; on Android through `/proc/self/fd`) or a
  folder. Copies only `tex1_*.png/.dds` inside the pack's `GEA` / `GEAE8P` / `GEAP8P` folder into
  `Load/Textures/GEA.importing/`, then swaps it in for `GEA/`; per-file CRCs checked; duplicates by
  name dropped; `skies-texture-pack.txt` there records the name and counts (textures, BC7).
- Launcher (`tools/moderngekko_launcher.cpp`, moderngekko-local patch): Settings, Graphics: "HD
  textures" On/Off (once a pack is imported), "Texture pack": Import a texture pack... (.7z/.zip,
  picker in Downloads) / Import a folder... / Remove. A new import turns HD textures on.
  `--import-texture-pack PATH` imports without the window (tests).
- In-game menu (`VideoCommon/SkiesMenu.cpp`): Graphics, "HD textures" (Off with a hint when nothing
  is imported); switching sets `GFX_HIRES_TEXTURES`, the texture cache reloads at its next frame.
- Android: `SkiesNative.importTexturePack / describeTexturePack / texturePackBc7 /
  removeTexturePack` (`jni/SkiesImport.cpp`), Settings, Graphics rows in `SkiesSettingsPanel.kt`,
  picker and import thread in `SkiesLauncherActivity.kt` (as the Dreamcast music's). Warns when the
  imported pack has BC7 textures.

## Status

- [x] Linux build, CLI import tests (2026-10-17, `--import-texture-pack` with `XDG_DATA_HOME` set to
      `~/projects/texture-packs/test/user-*`): DDS .7z 5.2 s, PNG .7z 4.2 s, PNG .zip 0.9 s, the
      unpacked folder and its GEA folder alone; all byte-identical to the pack (diff -r; the PNG
      pack's empty "(Placeholder)" folders aren't created). Both packs: 745 textures (PNG `_mipN`
      files not counted), DDS: 535 BC7. Cut-off .7z, non-archive, pack without GEA, missing file:
      each refused with its message, nothing left behind. Build with `TMPDIR` on disk when /tmp
      (tmpfs) is full.
- [x] Android build, Retroid check (2026-10-17 late; debug test app, ws23 modules): see *Android /
      Retroid* below. Two fixes came out of it (mGBA symbol clash, BC7 on Adreno).
- [x] Flight meter: the pack's `GEA/UI/Main/tex1_128x128_089930a5aca922cb_14` is its Standard (4:3)
      meter, byte for byte; the WideScreen one in `Optional` is the same art squeezed sideways, for
      cheat codes that stretch the HUD. The import takes `GEA` only (Standard). **But the port's
      widescreen stretches the world map's flight meter and compass** (Windows 2026-10-17,
      `C:\dev\SkiesHDTex\build\win-shots\worldmap`, state 10, original textures: at the same
      picture height the meter is ~1.33x as wide in 16:9 as in 4:3, the compass ~1.4x; the 4:3
      picture is 1.205:1, Dolphin's analog aspect, so 4:3 is the reference). They aren't on the HUD
      anchor path ("HUD at its original shape"). A game patch fix (ws24) would make Standard right in
      both modes; until then Standard matches the stretched original meter in 16:9. Reported to the
      user, not changed here.
- [x] The pack's "UI texture that needs Store EFB Copies to Texture Only" (its v0.02 GameSettings
      ini): with copies to RAM, which this game needs (Dolphin's own `GEA.ini`), the game reads its
      EFB copy back and Dolphin hashes it as a new texture each time, so a replacement can't match;
      that one element shows what the game draws. Not changed.
- [x] Patches: new `recompcore-hd-textures.patch` (last in the series), `moderngekko-local.patch`
      regenerated, `tools/check-patches.sh` passes; `patches/README.md` rows; player READMEs (HD
      textures sections), CREDITS lines and `release/THIRD-PARTY.md` (LZMA SDK, public domain).
- [x] Windows (2026-10-17 late): release build in `C:\dev\SkiesHDTex` (a tar copy of the worktree
      without build dirs, extracted with Windows' tar; `REGIONS=us eu`, configure says "LZMA SDK
      found"). `--import-texture-pack` of the DDS .7z with `LOCALAPPDATA=C:\dev\SkiesHDTex\build\la`:
      6.4 s, identical to the pack, HD textures on. Runner (`build\game-keys.ps1`: release runner,
      ws22 game + `modules-llvm-pgo-ws22b`, `--load-state`, real key presses, window shots): the
      first battle shows the pack's BC7 faces; menu (Esc, W = next tab, Down to HD textures, Right)
      switches them off at once. Launcher (`build\la-start.ps1`, `la-click.ps1`): Settings, Graphics
      shows HD textures + Import a texture pack... / Import a folder... / Remove; choosing On writes
      `HiresTextures = 1`. Shots `build\win-shots`, `build\la-shots`. The release runner has no
      automation directory, so `drive.py` can't drive it (use the keys script).
- [ ] Linux launcher UI by hand: not driven (another session's game window was up on WSLg); same
      ImGui code as Windows. The Linux CLI import is checked.
- [x] Committed on `hdtex`: the feature, then a docs pass (development-notes
      *HD textures* + *Known issues*, BUILDING, the other handoffs, the bug template).
- [ ] **Merge into `dev`: held by the user (2026-10-17) until the widescreen fix** for the world map's
      flight meter and compass (game patch ws24, development-notes *Known issues*). Then: merge (fast
      forward if `dev` hasn't moved), apply `recompcore-hd-textures.patch` in the main checkout,
      `tools/check-patches.sh`, and the release-note line in HANDOFF-release *Next*. After the fix,
      look at the HD meter in 16:9 again (the pack's `GEA` one is the 4:3 meter, which is then right).

## Android / Retroid (2026-10-17 late)

APK `build/android/apk/skies-hdtex-2026-10-17.apk` (debug test app `org.dolphinemu.dolphinemu.debug`,
`JNILIBS=.../jniLibs-ws23`), installed over the PGO session's test app (data kept). Packs from
`/sdcard/Download`, picked through Settings, Graphics, Import a texture pack... (the picker opens on
Recent; Downloads is one tap away).

1. **First DDS import: the app was killed for memory** (6.4 GB RSS in 2 s, nothing written).
   simpleperf page faults (no root needed for a debuggable app) put them all in
   `std::vector<optional<string>> targets(db.NumFiles)`. The Android app builds mGBA (desktop doesn't:
   ModernGekko sets `USE_MGBA OFF`), and mGBA links an older LZMA SDK with the same function names:
   `SzArEx_Open` was mGBA's, filling our `CSzArEx` with its own layout. Fix: `Skies7zPrefix.h`
   (`#define SzArEx_Open Skies7z_SzArEx_Open`, all 95 names the library defines on x86-64 and
   arm64), included before the SDK headers and forced into every SDK file. After it: DDS .7z 6 s,
   441 MB peak; PNG .7z 5 s.
2. **Pack name "1000044662"**: a Downloads pick is a media URI ending in a number. The JNI passes
   `GetAndroidContentDisplayName` as `Import`'s new `file_name`.
3. **BC7 on the stock driver** (Adreno 650, Qualcomm V@0746.0): it reports `textureCompressionBC`,
   so Dolphin uploaded the 535 BC7 textures, which sampled as coloured noise (all of `Characters/` is
   BC7: Vyse's face, costume, sword). New `DriverDetails::BUG_BROKEN_BPTC` (Vulkan, Qualcomm, Android)
   turns `bSupportsBPTCTextures` off: they are skipped and the game's own stay, as the settings text
   says. The 210 uncompressed DDS (UI, text) load. Turnip not tried.
4. PNG pack: first battle (`02-first-battle.sav`) at 1.003x / 59.9 fps, HD faces. Live switch
   (automation `config:Graphics.Settings.HiresTextures=False`; the system is `Graphics`, not `GFX`)
   changes the textures at once both ways. Remove deletes `GEA/` and sets HD textures off.
5. Left as found: textures removed, HiresTextures False, app closed, `stay_on_while_plugged_in` and
   `files/PGO/` untouched. The test app now has the hdtex build: the PGO session reinstalls its own.

Shots: `build/android/hdtex-shots/` (g-battle-on = DDS before the fix, g-battle-png,
g-battle-dds-fixed, toggle-face.png = on / off / on).
