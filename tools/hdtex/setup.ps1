# Creates the virtualenv this pipeline runs in, and fetches the Real-ESRGAN weights.
# Safe to re-run; it skips whatever is already in place.
#
#   .\setup.ps1              CUDA build of torch (this machine has an RTX 3060 Ti)
#   .\setup.ps1 -Cpu         CPU-only torch, ~200 MB instead of ~2.5 GB
#   .\setup.ps1 -NoWeights   environment only

[CmdletBinding()]
param(
    [switch]$Cpu,
    [switch]$NoWeights
)

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$venv = Join-Path $PSScriptRoot '.venv'
$py = Join-Path $venv 'Scripts\python.exe'

if (-not (Test-Path $py)) {
    # Pin the interpreter explicitly.  'py -3' picks the newest *registered*
    # CPython, which on this machine is 3.15.0a3 -- an alpha, for which no torch
    # wheel exists on any index.  Walk a list of versions torch actually ships
    # for, newest first, and take the first one installed.
    $wanted = '3.13', '3.12', '3.11', '3.10'
    $chosen = $null
    if (Get-Command py -ErrorAction SilentlyContinue) {
        foreach ($v in $wanted) {
            & py "-$v" -c "import sys" 2>$null
            if ($LASTEXITCODE -eq 0) { $chosen = $v; break }
        }
    }
    if (-not $chosen) {
        throw ("no suitable Python found.  torch publishes wheels for " +
               ($wanted -join ', ') + "; install one and re-run.")
    }
    Write-Host "==> creating .venv on Python $chosen" -ForegroundColor Cyan
    & py "-$chosen" -m venv $venv
    if (-not (Test-Path $py)) { throw "venv creation failed: no $py" }
} else {
    Write-Host '==> .venv already exists' -ForegroundColor DarkGray
}

& $py -m pip install --quiet --upgrade pip
if ($LASTEXITCODE -ne 0) { throw 'pip self-upgrade failed' }

Write-Host '==> installing numpy / pillow / tqdm' -ForegroundColor Cyan
& $py -m pip install --quiet -r requirements.txt
if ($LASTEXITCODE -ne 0) { throw 'requirements install failed' }

$hasTorch = & $py -c "import importlib.util,sys; sys.exit(0 if importlib.util.find_spec('torch') else 1)"; $torchPresent = ($LASTEXITCODE -eq 0)
if (-not $torchPresent) {
    if ($Cpu) {
        Write-Host '==> installing torch (CPU, ~200 MB)' -ForegroundColor Cyan
        & $py -m pip install torch --index-url https://download.pytorch.org/whl/cpu
    } else {
        Write-Host '==> installing torch (CUDA 12.4, ~2.5 GB -- this takes a while)' -ForegroundColor Cyan
        & $py -m pip install torch --index-url https://download.pytorch.org/whl/cu124
    }
    if ($LASTEXITCODE -ne 0) { throw 'torch install failed' }
} else {
    Write-Host '==> torch already installed' -ForegroundColor DarkGray
}

if (-not $NoWeights) {
    Write-Host '==> fetching Real-ESRGAN weights' -ForegroundColor Cyan
    & $py fetch_weights.py
    if ($LASTEXITCODE -ne 0) { throw 'weight download failed' }

    # NVEncC carries the NGX VSR network used for the cutscenes.  Windows only:
    # setup.sh deliberately does not do this, since macOS has no NVIDIA GPU.
    Write-Host '==> fetching NVEncC (NVIDIA VSR)' -ForegroundColor Cyan
    & $py fetch_nvencc.py
    if ($LASTEXITCODE -ne 0) { throw 'NVEncC download failed' }
}

Write-Host ''
& $py -c "import torch;print('torch',torch.__version__,'cuda',torch.cuda.is_available(), torch.cuda.get_device_name(0) if torch.cuda.is_available() else '')"
Write-Host ''
Write-Host 'ready.  next:' -ForegroundColor Green
Write-Host '    .\.venv\Scripts\python roundtrip.py      # prove the codecs'
Write-Host '    .\.venv\Scripts\python make_hd.py --all  # build GREED_HD.BLO'
