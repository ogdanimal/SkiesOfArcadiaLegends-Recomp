# Screenshot of one window (its client area), from WSL or Windows, without catching other windows:
# DPI-aware, so a scaled window comes out at full size. PrintWindow flag 3 = client area + full
# content (a plain PrintWindow gives black for SDL / GPU windows).
# usage: powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools\capture-window-win.ps1 -Name SkiesRecomp -Out C:\path\shot.png
param([string]$Name, [string]$Out)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { [DllImport("user32.dll")] public static extern bool SetProcessDPIAware(); [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
 public struct RECT { public int L, T, R, B; } }
"@
[W]::SetProcessDPIAware() | Out-Null
$p = Get-Process -Name $Name | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
$r = New-Object W+RECT; [W]::GetClientRect($p.MainWindowHandle, [ref]$r) | Out-Null
$bmp = New-Object System.Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
$g = [System.Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc()
[W]::PrintWindow($p.MainWindowHandle, $dc, 3) | Out-Null
$g.ReleaseHdc($dc); $bmp.Save($Out); "$($bmp.Width)x$($bmp.Height)"
