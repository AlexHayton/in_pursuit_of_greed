# Capture framebuffer dumps in both render modes, for the byte-identity checks.
#
#   .\shots.ps1 base        -> work\base_original_NNN.ppm, work\base_hd_NNN.ppm
#   .\shots.ps1 stage3
#
# Neither platform allows an ordinary screenshot (see CLAUDE.md), so this drives
# the in-process GREED_SHOT hook instead.  GREED_MODE forces the renderer mode,
# because the shipped SETUP.CFG predates the magic header and LoadSetup rejects
# it, so the built-in default would otherwise always win.

param(
    [Parameter(Mandatory = $true)][string]$Tag,
    # Early ticks only, by default.  The comparison drifts later on because
    # R_render.c gates a random call on frame *parity*
    # (`if (frameon&1) wallflicker4=MS_RndT()...`), so the flicker sequence
    # depends on frame rate, not on the tick clock.  On a loaded machine that
    # makes tick 600 differ run-to-run by ~1.6% of pixels with an identical
    # binary.  120 and 300 are reproducible; trust those for byte-identity.
    [string]$At = "120,300",
    [int]$Seconds = 25
)

$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$exe = Join-Path $root 'source_win64\build\Greed.exe'
$data = Join-Path $root 'greed_final'
$work = Join-Path $PSScriptRoot 'work'

if (-not (Test-Path $exe)) { throw "no $exe -- build first" }
New-Item -ItemType Directory -Force $work | Out-Null

foreach ($mode in 'original', 'hd') {
    $env:GREED_MODE = $mode
    $env:GREED_SHOT = Join-Path $work "${Tag}_${mode}_"
    $env:GREED_SHOT_AT = $At
    $log = Join-Path $work "${Tag}_${mode}.log"

    Push-Location $data
    $p = Start-Process -FilePath $exe -ArgumentList '-nointro', '-window' `
        -PassThru -RedirectStandardOutput $log `
        -RedirectStandardError "$log.err"
    Start-Sleep -Seconds $Seconds
    if (-not $p.HasExited) { $p.Kill(); $p.WaitForExit() }
    Pop-Location

    $shots = @(Get-ChildItem "$work\${Tag}_${mode}_*.ppm" -ErrorAction SilentlyContinue)
    Write-Host "  $mode : $($shots.Count) shots"
    if ($shots.Count -eq 0) { Get-Content $log -Tail 8 }
}

Remove-Item Env:\GREED_MODE, Env:\GREED_SHOT, Env:\GREED_SHOT_AT -ErrorAction SilentlyContinue
