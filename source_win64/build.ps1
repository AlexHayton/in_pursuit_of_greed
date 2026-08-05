# Configure and build the Windows port.
#
# clang-cl, cmake and ninja all ship with Visual Studio 2022 but none of them
# are on PATH, so this imports the VS developer environment first.  clang-cl is
# the compiler rather than cl because it is the same compiler family the macOS
# port is built with: -std=gnu90 and the four -Werror=*-cast guards in
# cmake/engine.cmake transfer verbatim, and those caught six real
# pointer-truncation bugs during that port.
#
#   .\build.ps1                  # Release  -> build/
#   .\build.ps1 -Config Debug    # Debug    -> build-debug/
#   .\build.ps1 -Clean           # wipe the tree first (refetches dependencies)
#
# -Config rather than -Debug: PowerShell reserves the latter as a common
# parameter and refuses to load a script that redefines it.

param(
    [ValidateSet('Release', 'Debug')]
    [string]$Config = 'Release',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
        -latest -products * -property installationPath
if (-not $vs) { throw "Visual Studio 2022 not found." }

$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'

foreach ($tool in @($cmake, $ninja)) {
    if (-not (Test-Path $tool)) { throw "Missing: $tool" }
}

# Prefer clang-cl when it is present -- it is the same compiler family the
# macOS port is built with, so cmake/engine.cmake can use the identical
# -Werror=*-cast pointer-truncation guards.  It ships with VS only if the
# "C++ Clang tools for Windows" component was selected, and with a standalone
# LLVM otherwise; plain cl is the fallback and engine.cmake maps the guards
# onto the equivalent /we numbers.
$cc = $null
foreach ($cand in @(
    (Join-Path $vs 'VC\Tools\Llvm\x64\bin\clang-cl.exe'),
    (Join-Path $vs 'VC\Tools\Llvm\bin\clang-cl.exe'),
    'C:\Program Files\LLVM\bin\clang-cl.exe')) {
    if (Test-Path $cand) { $cc = $cand; break }
}

# VsDevCmd.bat shells out to a bare `vswhere`, so without the installer
# directory on PATH it prints "not recognized" before carrying on regardless.
$env:PATH = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer;$env:PATH"

# -SkipAutomaticLocation so this does not chdir out from under us.
Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation `
                 -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

Set-Location $PSScriptRoot

if ($Config -eq 'Debug') { $dir = 'build-debug' } else { $dir = 'build' }

if ($Clean -and (Test-Path $dir)) { Remove-Item -Recurse -Force $dir }

# Single-config Ninja rather than the multi-config VS generator, so that
# build/ means Release and build-debug/ means Debug exactly as on macOS, and
# `cmake --build build` means the same thing on both platforms.
$cmakeArgs = @("-G", "Ninja", "-B", $dir,
          "-DCMAKE_BUILD_TYPE=$Config",
          "-DCMAKE_MAKE_PROGRAM=$ninja")
if ($cc) {
    Write-Host "Compiler: clang-cl ($cc)" -ForegroundColor Cyan
    $cmakeArgs += "-DCMAKE_C_COMPILER=$cc"
} else {
    Write-Host "Compiler: MSVC cl (clang-cl not installed)" -ForegroundColor Yellow
}

& $cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "configure failed" }

& $cmake --build $dir -j 8
if ($LASTEXITCODE -ne 0) { throw "build failed" }

Write-Host ""
Write-Host "Built $dir\Greed.exe" -ForegroundColor Green
