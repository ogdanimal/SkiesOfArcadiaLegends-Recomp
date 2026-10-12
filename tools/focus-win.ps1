# Bring the moderngekko-run window to the foreground and optionally close it the way a user would.
# usage: powershell -NoProfile -ExecutionPolicy Bypass -File tools\focus-win.ps1 -Action focus|close|altf4
#   focus: foreground only; close: foreground, then the title bar's X (WM_SYSCOMMAND SC_CLOSE);
#   altf4: foreground, then real Alt+F4 key presses. Prints "focused=1" when GetForegroundWindow is the
#   game window just before the close (Windows' foreground lock can refuse a process launched from WSL).
param([ValidateSet('focus', 'close', 'altf4')][string]$Action = 'focus')

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class W {
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, IntPtr pid);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool attach);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
}
'@

$proc = Get-Process | Where-Object { $_.ProcessName -like 'moderngekko-run*' -and $_.MainWindowHandle -ne 0 } |
  Select-Object -First 1
if (-not $proc) { Write-Output 'focused=0 (no window)'; exit 1 }
$hwnd = $proc.MainWindowHandle

for ($i = 0; $i -lt 5 -and [W]::GetForegroundWindow() -ne $hwnd; $i++) {
  $fg = [W]::GetForegroundWindow()
  $fgThread = [W]::GetWindowThreadProcessId($fg, [IntPtr]::Zero)
  $me = [W]::GetCurrentThreadId()
  [void][W]::AttachThreadInput($me, $fgThread, $true)
  [void][W]::ShowWindow($hwnd, 9)  # SW_RESTORE
  [void][W]::BringWindowToTop($hwnd)
  [void][W]::SetForegroundWindow($hwnd)
  [void][W]::AttachThreadInput($me, $fgThread, $false)
  Start-Sleep -Milliseconds 200
}
$focused = [int]([W]::GetForegroundWindow() -eq $hwnd)
Write-Output "focused=$focused"

if ($Action -eq 'close') {
  [void][W]::PostMessage($hwnd, 0x0112, [IntPtr]0xF060, [IntPtr]::Zero)  # WM_SYSCOMMAND, SC_CLOSE
} elseif ($Action -eq 'altf4') {
  [W]::keybd_event(0x12, 0x38, 0, [UIntPtr]::Zero)  # Alt down
  [W]::keybd_event(0x73, 0x3E, 0, [UIntPtr]::Zero)  # F4 down
  [W]::keybd_event(0x73, 0x3E, 2, [UIntPtr]::Zero)  # F4 up
  [W]::keybd_event(0x12, 0x38, 2, [UIntPtr]::Zero)  # Alt up
}
if (-not $focused) { exit 2 }
