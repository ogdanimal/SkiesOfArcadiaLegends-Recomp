# DRAFT: ModernGekko issue (not filed)

Target: https://github.com/ExpansionPak/ModernGekko/issues
Checked against ModernGekko `origin/master` (`a2e4e2b`) on 2026-10-07. `InspectGame` still always hashes
`files/`, and `Runtime::Create` still calls it again.

---

**Title:** moderngekko-run spends 10–30 s at startup hashing the whole game directory, twice

**Summary**

`InspectGame` (`src/runtime/game.cpp`) always computes `assets_sha256`. To do that, it reads every file
under `files/` fully into memory and SHA-256-hashes it. `moderngekko-run` calls `InspectGame`, and then
`Runtime::Create` (`dolphin_runtime.cpp`) calls it again, so the whole disc is hashed twice before the
first log line or the window appears.

For Skies of Arcadia Legends (`files/` is 1.4 GB), time from launch to the game running:

| | before | after skipping the hash |
|---|---|---|
| Windows 11 (MSVC Release) | ~28 s | ~2 s |
| Linux / WSL2 (GCC 15) | 12.4 s | 2.7 s |

That's the same with or without `--load-state`. On Windows, the process sits at ~28 MB with one core
busy and no window for ~27 s, which looks like a hang. We killed it as one at first.

The result is only read when a release build sets `MODERNGEKKO_REQUIRED_ASSETS_SHA256`, and for netplay
compatibility. Netplay (`netplay_session.cpp`) and the launcher call `InspectGame` themselves.

**Fix we're using** (keeps the API's default behavior):

- `InspectGame(const std::filesystem::path& root, bool hash_assets = true)`. With `false`,
  `assets_sha256` is left empty.
- `tools/moderngekko_run.cpp` passes `true` only `#ifdef MODERNGEKKO_REQUIRED_ASSETS_SHA256`.
- `Runtime::Create` passes `false`, because it only uses the disc ID and the name.

```diff
-  const auto inspected = moderngekko::InspectGame(config.game_root);
+#ifdef MODERNGEKKO_REQUIRED_ASSETS_SHA256
+  constexpr bool hash_assets = true;
+#else
+  // Nothing else here reads assets_sha256, and hashing every game file
+  // costs tens of seconds per launch.
+  constexpr bool hash_assets = false;
+#endif
+  const auto inspected = moderngekko::InspectGame(config.game_root, hash_assets);
```

Other options, if you'd rather keep the hash on every launch: cache it keyed by (path, size, mtime);
stream files through the hasher instead of `ReadFileBytes` (that saves memory, not time); or use a
SHA-NI/ARMv8 SHA-256 implementation. Pinned release builds would still pay the cost twice unless
`Runtime::Create` stops re-hashing.
