# LZMA SDK (7z decoder only)

The C 7z decoder from Igor Pavlov's LZMA SDK **23.01** (2023-06-20), used by the HD texture pack
import (`VideoCommon/TexturePackImport.cpp`) to read `.7z` packs. Only the files the SDK's own
7z decoder sample (`C/Util/7z`) builds are here, plus the SDK's C headers; nothing is changed.

- Source: https://www.7-zip.org/a/lzma2301.7z
  (sha256 `317dd834d6bbfd95433488b832e823cd3d4d420101436422c03af88507dd1370`), folder `C/`.
- Licence: public domain ("LZMA SDK is placed in the public domain", https://www.7-zip.org/sdk.html).

libchdr's own LZMA decoder (`lib/libchdr/deps/lzma-26.02`) renames its symbols (`CHDR_LzmaDec_*`),
so both link into the same program. mGBA's older copy of the SDK (Dolphin's `Externals/mGBA`, built
into the Android app) doesn't, so this one is compiled under `Skies7z_` names
(`VideoCommon/Skies7zPrefix.h`, forced into every file here by `VideoCommon/CMakeLists.txt`).
