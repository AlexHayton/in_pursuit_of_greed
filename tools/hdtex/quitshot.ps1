# Capture the QUIT dialog sliding down the main menu, in either render mode.
#
#   .\quitshot.ps1 after            -> work\after_NNN.ppm
#   .\quitshot.ps1 orig -Mode original
#
# Drives the menu with GREED_KEYS, because the dialog cannot otherwise be
# reached without a human at the keyboard (see the note on the hook in
# sys_main.c).  Main menu cursor 0 is NEW and 1 is QUIT, so one DOWNARROW then
# ENTER opens ShowQuit.  It slides from y=-66 to y=67 at 2 logical rows a tick,
# i.e. about 67 ticks, and the trail shows while it is moving -- so the shots
# are spaced across the slide, not just at the end.

param(
    [Parameter(Mandatory = $true)][string]$Tag,
    [ValidateSet('original', 'hd')][string]$Mode = 'hd',
    [string]$Keys = "60:0x50:6,80:0x1c:6",
    [string]$At = "100,120,140,200",
    [int]$Seconds = 20
)

$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$exe = Join-Path $root 'source_win64\build\Greed.exe'
$data = Join-Path $root 'greed_final'
$work = Join-Path $PSScriptRoot 'work'

if (-not (Test-Path $exe)) { throw "no $exe -- build first" }
New-Item -ItemType Directory -Force $work | Out-Null
Get-ChildItem "$work\${Tag}_*.ppm" -ErrorAction SilentlyContinue | Remove-Item -Force

$env:GREED_MODE = $Mode
$env:GREED_KEYS = $Keys
$env:GREED_SHOT = Join-Path $work "${Tag}_"
$env:GREED_SHOT_AT = $At
$log = Join-Path $work "${Tag}.log"

Push-Location $data
$p = Start-Process -FilePath $exe -ArgumentList '-nointro', '-window' `
    -PassThru -RedirectStandardOutput $log -RedirectStandardError "$log.err"
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit() }
Pop-Location

Remove-Item Env:\GREED_MODE, Env:\GREED_KEYS, Env:\GREED_SHOT, Env:\GREED_SHOT_AT `
    -ErrorAction SilentlyContinue

$shots = @(Get-ChildItem "$work\${Tag}_*.ppm" -ErrorAction SilentlyContinue)
Write-Host "  $Tag ($Mode): $($shots.Count) shots"
if ($shots.Count -eq 0) { Get-Content $log -Tail 12 }
