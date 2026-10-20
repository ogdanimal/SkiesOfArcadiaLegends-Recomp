# Uncensored: the manifest generator

The Uncensored setting (`DiscIO/SkiesUncensor.h` in `patches/recompcore-uncensor.patch`) swaps in the
Japanese Dreamcast originals (Eternal Arcadia v1.009) of the models and textures the Western releases
changed. The recomp ships no game data: it ships `Data/Sys/Skies/uncensor-manifest.json` (names,
entry indices and SHA-1s), and at import converts the player's own Japanese discs' files with
`DiscIO/SkiesUncensorConvert.cpp`. These scripts make that manifest and the reference outputs the C++
port is tested against.

## Inputs

`$SKIES_RESEARCH` (default `~/projects/GameCube-Research/targets/SkiesOfArcadiaLegends`) holds the
extracted discs:

| Folder | Disc |
|---|---|
| `dcjp/files1`, `dcjp/files2` | Eternal Arcadia v1.009 discs 1 and 2 (Redump track 1 MD5s 2d53dfff…, b83bd0f8…) |
| `dc/files1`, `dc/files2` | the US Dreamcast disc 1 and a Western disc 2 (the reference for what the West changed) |
| `disc/files`, `eu/disc/files`, `jp/disc/files` | the GameCube discs GEAE8P, GEAP8P, GEAJ8P |

The US Dreamcast discs are only needed here: the manifest records what to change, so players import
only the Japanese discs.

## Rules (uncensor.py)

For each Dreamcast file that differs between the Japanese and US discs and has a GameCube counterpart:

- A model (object block) is replaced only when the US Dreamcast block converts byte for byte into the
  GameCube one (so it isn't a model Legends remade), and the block names a character texture (`tc…`)
  or uses a texture the US release added (the tavern sign's `tt04a500/501`). Entries are matched by
  table ID and name; duplicates by content.
- Character textures (`tc…`) that differ are re-encoded from the Japanese PVR in the GameCube
  original's own format (CMPR, C4/C8 with their palette, RGB5A3), headers and size kept. A Japanese-only
  texture is added (CMPR) only if a swapped-in model uses it. `ts…` textures are Japanese text: left.
- Textures the US release added that nothing uses any more are blanked (pixels zeroed) so the file
  still compresses into the original's size.
- The result is recompressed (AKLZ) to exactly the original's byte count: the game's decoder stops on
  input exhaustion, and a save state keeps the disc's file table, so a different size would be read at
  the wrong place.

Not covered (need motion or script work): Vigoro's shirtless model (79 bones against 50), Gilder's
victory pose (`beff/pcwin.mlk`, an effect format), the drunk sailor and other characters only the
Japanese disc places (script), Ramirez's blood (script), the drunk sailor model Legends remade.

## Running

```sh
python3 make_manifest.py OUTDIR            # all three games; OUTDIR/<usa|eu|jp>/... + uncensor-manifest.json
python3 inventory.py OUTDIR                # what converts and why not (report.tsv)
g++ -std=c++20 -O2 -I ../../lib/ModernGekko/vendor/dolphin/Source/Core \
    -I ../../lib/ModernGekko/vendor/dolphin/Externals/picojson convert_test.cpp \
    ../../lib/ModernGekko/vendor/dolphin/Source/Core/DiscIO/SkiesUncensorConvert.cpp -o convert_test
./convert_test                             # the C++ port against $SKIES_RESEARCH/uncensor/manifest
```

Ship the manifest compact (`json.dumps(m, separators=(',', ':'), sort_keys=True)`) as
`lib/ModernGekko/vendor/dolphin/Data/Sys/Skies/uncensor-manifest.json`, then regenerate
`patches/recompcore-uncensor.patch` (`tools/regen-recompcore-patch.sh uncensor`).
