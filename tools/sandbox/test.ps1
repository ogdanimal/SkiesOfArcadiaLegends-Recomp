# Clean-machine test of the Windows release zip, run inside Windows Sandbox at logon
# (skies-sandbox.wsb). Everything it learns goes to C:\out (the host's build\sandbox-test\out).
$ErrorActionPreference = 'Continue'
$out = 'C:\out'
function Log($m) { "$(Get-Date -Format HH:mm:ss) $m" | Add-Content "$out\test.log" }
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
function Shot($name) {
  $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
  $bmp = New-Object System.Drawing.Bitmap $b.Width, $b.Height
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Location, [System.Drawing.Point]::Empty, $b.Size)
  $bmp.Save("$out\$name.png"); $g.Dispose(); $bmp.Dispose()
}
Log 'start'
Get-ChildItem C:\Windows\System32\vcruntime140*.dll, C:\Windows\System32\msvcp140*.dll -ErrorAction SilentlyContinue |
  ForEach-Object { Log "system CRT present: $($_.Name)" }
$zip = Get-ChildItem C:\in\*.zip | Select-Object -First 1
Log "zip: $($zip.Name)"
Expand-Archive $zip.FullName C:\skies -Force
$dir = Get-ChildItem C:\skies -Directory | Select-Object -First 1
Set-Location $dir.FullName
$disc = Get-ChildItem C:\disc\*.rvz | Select-Object -First 1
Log "import: $($disc.Name)"
$p = Start-Process -FilePath '.\Skies of Arcadia Legends.exe' -ArgumentList '--extract', "`"$($disc.FullName)`"" `
  -RedirectStandardOutput "$out\extract.out" -RedirectStandardError "$out\extract.err" -Wait -PassThru
Log "import exit: $($p.ExitCode)"
$ud = Join-Path $env:LOCALAPPDATA 'Skies of Arcadia Legends'
Get-ChildItem $ud -Recurse -Filter main.dol | ForEach-Object { Log "DOL $($_.FullName) sha256 $((Get-FileHash $_.FullName).Hash.ToLower())" }
# The launcher's own window (home screen)
$l = Start-Process -FilePath '.\Skies of Arcadia Legends.exe' -PassThru
Start-Sleep 12; Shot 'launcher'; Stop-Process $l -Force -ErrorAction SilentlyContinue
$game = (Get-Content (Join-Path $ud 'default-game.txt') -ErrorAction SilentlyContinue | Select-Object -First 1)
if (-not $game) { $game = (Get-ChildItem $ud -Recurse -Filter main.dol | Select-Object -First 1).Directory.Parent.FullName }
Log "game: $game"
# The runner as the launcher starts it (backend from config.ini, Vulkan by default). Sandbox has no
# usable Vulkan device, so this checks the OpenGL fallback; Dolphin's alert dialogs off so nothing
# waits for a click.
$ini = Join-Path $ud 'Config\Dolphin.ini'
New-Item -ItemType Directory -Force (Split-Path $ini) | Out-Null
"`n[Interface]`nUsePanicHandlers = False`n" | Add-Content $ini
Log "config.ini before: $((Select-String -Path (Join-Path $ud 'config.ini') -Pattern '^backend=').Line)"
$r = Start-Process -FilePath '.\skies-run.exe' -ArgumentList '--game', "`"$game`"", '--user-dir', "`"$ud`"" `
  -RedirectStandardOutput "$out\run.out" -RedirectStandardError "$out\run.err" -PassThru
Start-Sleep 60
if ($r.HasExited) { Log "runner exited: $($r.ExitCode)" } else {
  Shot 'game'; Start-Sleep 30; Shot 'game-later'
  Get-Process skies-run -ErrorAction SilentlyContinue | ForEach-Object { Log "window: $($_.MainWindowTitle)" }
  Get-Process skies-run -ErrorAction SilentlyContinue | Stop-Process -Force
}
Log "config.ini after: $((Select-String -Path (Join-Path $ud 'config.ini') -Pattern '^backend=').Line)"
Copy-Item (Join-Path $ud 'Logs\*') $out -ErrorAction SilentlyContinue
Log 'done'
