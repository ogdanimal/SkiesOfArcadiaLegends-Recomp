# Third-party software

Skies of Arcadia Legends: Recompiled is distributed under the GNU General Public License, version 3
(`COPYING`). It includes or links the following:

- [ModernGekko](https://github.com/ExpansionPak/ModernGekko) (runtime and launcher): GPL-3.0
- [DolRecomp](https://github.com/ExpansionPak/DolRecomp) (static recompiler; its runtime support
  code is linked into the game module): GPL-3.0
- [Dolphin](https://github.com/dolphin-emu/dolphin) (video, audio, input and hardware code, through
  ModernGekko's RecompCore fork): GPL-2.0-or-later, used here under GPL-3.0
- [{fmt}](https://github.com/fmtlib/fmt): MIT

Dolphin's own third-party components (SDL, Dear ImGui, zlib-ng, Zstandard, cubeb, mbed TLS and others)
are listed with their licences in `THIRD-PARTY-DOLPHIN.md`, a copy of Dolphin's `Externals/licenses.md`.

The source code for this release, including every change made to the projects above, is available
from the project's repository.

Fonts are not bundled: the launcher and the in-game menu use the system's own fonts (Segoe UI on
Windows, Roboto on Android).

The launcher background art (I_LIKE_SONIC) and the app icon art (Reckoner) are from SteamGridDB and
belong to their artists; they aren't covered by the GPL.
