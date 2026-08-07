# Capture framebuffer dumps from inside a cutscene.
#
#   .\introshot.ps1 esrgan
#   .\introshot.ps1 vsr -At 300,900,1500 -Seconds 45
#
# shots.ps1 passes -nointro, which returns out of MissionBriefing before any FLI
# plays, so it proves nothing about the cutscene path.  This one deliberately
# does not.  DemoIntro probes MOVIES/TEXT.FLI and hands straight to
# DemoIntroFlis when it exists (Intro.c:477), so the movies start within a few
# seconds of launch; TEXT.FLI is 390 frames at speed 5, i.e. 1950 ticks.
#
# Neither platform allows an ordinary screenshot (see CLAUDE.md), hence the
# in-process GREED_SHOT hook and -window.

param(
    [Parameter(Mandatory = $true)][string]$Tag,
    [string]$At = "300,700,1100,1500",
    [int]$Seconds = 45,
    [ValidateSet('original', 'hd')][string]$Mode = 'hd'
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
$env:GREED_SHOT = Join-Path $work "${Tag}_"
$env:GREED_SHOT_AT = $At
$log = Join-Path $work "${Tag}.log"

Push-Location $data
$p = Start-Process -FilePath $exe -ArgumentList '-window' `
    -PassThru -RedirectStandardOutput $log -RedirectStandardError "$log.err"
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit() }
Pop-Location

Remove-Item Env:\GREED_MODE, Env:\GREED_SHOT, Env:\GREED_SHOT_AT -ErrorAction SilentlyContinue

$shots = @(Get-ChildItem "$work\${Tag}_*.ppm" -ErrorAction SilentlyContinue)
Write-Host "  $Tag ($Mode): $($shots.Count) shots"
$shots | ForEach-Object { Write-Host "    $($_.Name)" }
if ($shots.Count -eq 0) { Get-Content $log -Tail 12 }
