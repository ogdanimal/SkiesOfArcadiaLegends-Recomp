# DRAFT: ModernGekko issue (not filed)

Target: https://github.com/ExpansionPak/ModernGekko/issues. The code is in RecompCore
(`Source/Core/DolphinNoGUI/PlatformWin32.cpp`), but RecompCore has issues disabled, so either file it here
or open a PR against RecompCore.
Checked on 2026-10-07: ModernGekko master's RecompCore pin (`cf8ceb81b4`) still has the unconditional
`WM_KILLFOCUS` handler and a `DestroyWindow` without detaching. RecompCore `origin/main` (`78bdce82af`) has
a much smaller `PlatformWin32.cpp` with no `WM_KILLFOCUS` handler, so it may already be gone there. Check
which branch is current before filing.

---

**Title:** Windows: access violation on exit when the game window has keyboard focus; losing focus re-enables the frame limiter

**Summary**

Two problems in `PlatformWin32.cpp` (DolphinNoGUI):

1. **Crash on exit.** `~PlatformWin32` calls `DestroyWindow` while `GWLP_USERDATA` still points at the
   platform. When the window has keyboard focus, `DestroyWindow` sends `WM_KILLFOCUS`, and by then the
   runtime has already shut `Config` down. The handler's `Config::SetCurrent(MAIN_EMULATION_SPEED, 1.0f)`
   faults (`0xc0000005`, then `0xc000041d`, an exception escaping a window callback). In an A/B test with
   the window really focused, the unpatched runtime crashed 30 of 30 exits (X button, Alt+F4, and a
   programmatic stop alike). The patched runtime exited cleanly in 65 of 65.
   - Tip for reproducing: a process launched from another terminal (or from WSL) often never gets the
     foreground, so it never receives `WM_KILLFOCUS` and exits cleanly. Click into the window first.
2. **Focus loss overrides the configured speed.** The same `WM_KILLFOCUS` handler (and Space key-up) set
   `EmulationSpeed` to 1.0 unconditionally. With `EmulationSpeed = 0` (unlimited) in the user's config,
   alt-tabbing away silently turns the frame limiter back on.

**Fix we're using** (the full patch is below):

- In the destructor, call `SetWindowLongPtr(m_hwnd, GWLP_USERDATA, 0)` before `DestroyWindow`.
- At the top of `WndProc`, when `platform` is null and `msg != WM_NCCREATE`, return `DefWindowProc(...)`.
  This also covers the F1/F11/Esc/Alt+Enter/`WM_CLOSE` handlers, which dereference `platform` unchecked.
- Space fast-forward remembers the speed it replaced (`std::optional<float>`). Space key-up and
  `WM_KILLFOCUS` restore that speed, and only if a fast-forward is active.

```diff
 PlatformWin32::~PlatformWin32()
 {
   if (m_hwnd)
+  {
+    // Detach first: destroying a focused window sends WM_KILLFOCUS, and by now the runtime has shut
+    // Config down, so the handler must not touch it (an access violation on exit otherwise).
+    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, 0);
     DestroyWindow(m_hwnd);
+  }
 }
@@ WndProc
   PlatformWin32* platform = reinterpret_cast<PlatformWin32*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
+  // Messages before WM_NCCREATE and after the destructor detaches the window get the default handling.
+  if (!platform && msg != WM_NCCREATE)
+    return DefWindowProc(hwnd, msg, wParam, lParam);
@@ VK_SPACE down
+      if (!platform->m_speed_before_fast_forward)
+        platform->m_speed_before_fast_forward = Config::Get(Config::MAIN_EMULATION_SPEED);
       Config::SetCurrent(Config::MAIN_EMULATION_SPEED, FAST_FORWARD_SPEED);
@@ WM_KEYUP (VK_SPACE) and WM_KILLFOCUS
-      Config::SetCurrent(Config::MAIN_EMULATION_SPEED, 1.0f);
+      platform->EndFastForward();   // restores *m_speed_before_fast_forward, if set
```

Tested on Windows 11 with an MSVC-built `moderngekko-run.exe`, running both a C-backend and an
LLVM-backend module.
