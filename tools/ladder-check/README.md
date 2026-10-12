# Ladder glitch checks (2026-10-10)

Scripts from the session that found the Retroid's green bands (NOTES *Texture garbage in in-betweens ...
and green bands*). Paths assume the repo layout and the Windows tree at `C:\dev\SkiesRecomp`.

- `devladder.sh TAG`: boot the Retroid build, load `build/savestates/15-ladder.sav`, `screenrecord` 16 s
  while holding down, pull to `build/android/recordings-2026-10-10/check-ladder-${PFX}-TAG.mp4`, score.
- `colorfind.py VIDEO`: frames with green (YUV-zero) / magenta / cyan / yellow areas (the paint tests).
- `greenfind.py VIDEO`: green-band frames only. `outlier.py VIDEO [TH]`: single-frame garbage (a frame
  differing from both neighbours where they agree).
- `cloudfind.py VIDEO [DELTA]`: the dark cloud-sea events (the region below the ladder darker than the
  video's median by DELTA, default 25; one event = consecutive frames). Reproduces the user's list of
  2026-10-10 exactly; `clouddump.py DIR [DELTA]` is the same over a Windows dump, real and in-between pictures
  listed separately.
- `bisect.sh LO HI`: on the device, skip replay draws LO..HI-1 (`FrameInterpolation.skip`) for one run.
- `wintest.sh` / `ladrun.sh TAG USERDIR [ENV..]` / `menurun.sh TAG [EXE]` / `dumpscore.py DIR`: Windows
  runs with every picture dumped and the in-betweens scored against their real neighbours (`ladrun.sh`
  also runs `clouddump.py`).

Device runs vary a lot (7-44 green per run on the old build): never compare single runs.

Real or in-between? A `FrameInterpolation.skip` range that drops the HUD from the replay (3500 6000 here)
marks the in-betweens in a recording: frames without the minimap. That settled that the cloud-sea garbage
was in the game's own picture (2026-10-10 night).
