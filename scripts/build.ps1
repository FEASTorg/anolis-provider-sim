#!/usr/bin/env pwsh
# Build anolis-provider-sim (PowerShell)
#
# Usage:
#   .\scripts\build.ps1 [options]
#
# Options:
#   -Clean                 Remove build directory before configure
#   -BuildDebug            Build Debug (default: Release)
#   -Release               Build Release
#   -TSan                  Enable ThreadSanitizer (Linux/macOS only)
#   -BuildDir <path>       Override build directory
#   -Generator <name>      CMake generator (default: VS on Windows, Ninja elsewhere)
#   -Jobs <N>              Parallel build jobs
#   -WithFluxGraph         Enable FluxGraph support (default: OFF)
#   -WithoutFluxGraph      Disable FluxGraph support
#   -FluxGraphDir <path>   FluxGraph repo path (used only with -WithFluxGraph)
#   -DNAME=VALUE           Pass CMake definitions (e.g., -DENABLE_FLUXGRAPH=OFF)
#   -Help                  Show this help

[CmdletBinding(PositionalBinding = $false)]
param(
    [switch]$Clean,
    [switch]$BuildDebug,
    [switch]$Release,
    [switch]$TSan,
    [string]$BuildDir,
    [string]$Generator,
    [int]$Jobs,
    [switch]$WithFluxGraph,
    [switch]$WithoutFluxGraph,
    [string]$FluxGraphDir,
    [switch]$Help,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraArgs
)

$ErrorActionPreference = "Stop"

# Cross-edition OS detection:
# - Windows PowerShell 5.1 (PSEdition=Desktop) only runs on Windows.
# - PowerShell 7+ (PSEdition=Core) provides $IsWindows/$IsMacOS/$IsLinux.
if ($PSVersionTable.PSEdition -eq "Desktop") {
    $onWindows = $true
    $onMacOS = $false
    $onLinux = $false
}
else {
    $onWindows = [bool]$IsWindows
    $onMacOS = [bool]$IsMacOS
    $onLinux = [bool]$IsLinux
}

# Support GNU-style long options (e.g. --clean, --build-dir build).
# Collect CMake arguments (-D flags) separately to pass through.
$cmakeArgs = @()
for ($i = 0; $i -lt $ExtraArgs.Count; $i++) {
    $arg = $ExtraArgs[$i]
    switch -Regex ($arg) {
        '^--clean$' { $Clean = $true; continue }
        '^--debug$' { $BuildDebug = $true; continue }
        '^--release$' { $Release = $true; continue }
        '^--tsan$' { $TSan = $true; continue }
        '^--help$' { $Help = $true; continue }
        '^--build-dir$' {
            if ($i + 1 -ge $ExtraArgs.Count) { throw "--build-dir requires a value" }
            $i++
            $BuildDir = $ExtraArgs[$i]
            continue
        }
        '^--build-dir=(.+)$' { $BuildDir = $Matches[1]; continue }
        '^--generator$' {
            if ($i + 1 -ge $ExtraArgs.Count) { throw "--generator requires a value" }
            $i++
            $Generator = $ExtraArgs[$i]
            continue
        }
        '^--generator=(.+)$' { $Generator = $Matches[1]; continue }
        '^(-j|--jobs)$' {
            if ($i + 1 -ge $ExtraArgs.Count) { throw "$arg requires a value" }
            $i++
            $Jobs = [int]$ExtraArgs[$i]
            continue
        }
        '^--jobs=(.+)$' { $Jobs = [int]$Matches[1]; continue }
        '^--with-fluxgraph$' { $WithFluxGraph = $true; continue }
        '^--without-fluxgraph$' { $WithoutFluxGraph = $true; continue }
        '^--fluxgraph-dir$' {
            if ($i + 1 -ge $ExtraArgs.Count) { throw "--fluxgraph-dir requires a value" }
            $i++
            $FluxGraphDir = $ExtraArgs[$i]
            continue
        }
        '^--fluxgraph-dir=(.+)$' { $FluxGraphDir = $Matches[1]; continue }
        '^-D' {
            # CMake definition flag - pass through
            $cmakeArgs += $arg
            continue
        }
        default { throw "Unknown argument: $arg" }
    }
}

if ($Help) {
    Get-Content $MyInvocation.MyCommand.Path | Select-Object -First 40
    exit 0
}

# Backward compatibility:
# With CmdletBinding, -Debug is a common parameter. Treat it as a request
# for Debug build when explicitly passed.
if ($PSBoundParameters.ContainsKey("Debug")) {
    $BuildDebug = $true
}

if ($BuildDebug -and $Release) {
    Write-Host "[ERROR] Use only one of -BuildDebug/--debug or -Release." -ForegroundColor Red
    exit 2
}

if ($WithFluxGraph -and $WithoutFluxGraph) {
    Write-Host "[ERROR] Use only one of -WithFluxGraph/--with-fluxgraph or -WithoutFluxGraph/--without-fluxgraph." -ForegroundColor Red
    exit 2
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir

$buildType = if ($BuildDebug) { "Debug" } else { "Release" }
if ($Release) { $buildType = "Release" }

if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot ($(if ($TSan) { "build-tsan" } else { "build" }))
}

if ($TSan -and $onWindows) {
    Write-Host "[ERROR] -TSan is not supported on Windows/MSVC." -ForegroundColor Red
    Write-Host "Use Linux/macOS for TSAN builds." -ForegroundColor Yellow
    exit 1
}

if (-not $env:VCPKG_ROOT) {
    $candidates = @(
        "$HOME/tools/vcpkg",
        "$HOME/vcpkg",
        "$env:USERPROFILE\vcpkg",
        "C:\tools\vcpkg",
        "C:\vcpkg"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path (Join-Path $candidate "scripts/buildsystems")) {
            $env:VCPKG_ROOT = $candidate
            break
        }
    }
}

$vcpkgRoot = $env:VCPKG_ROOT
if (-not $vcpkgRoot) {
    Write-Host "[ERROR] VCPKG_ROOT is not set." -ForegroundColor Red
    Write-Host "Set VCPKG_ROOT to your vcpkg installation path." -ForegroundColor Yellow
    exit 1
}

$toolchainFile = Join-Path $vcpkgRoot "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path $toolchainFile)) {
    Write-Host "[ERROR] VCPKG_ROOT is not set or invalid." -ForegroundColor Red
    Write-Host "Set VCPKG_ROOT to your vcpkg installation path." -ForegroundColor Yellow
    exit 1
}

$triplet = "x64-linux"
if ($onWindows) {
    $triplet = "x64-windows"
}
elseif ($onMacOS) {
    $triplet = "x64-osx"
}
if ($TSan) {
    $triplet = "x64-linux-tsan"
}

$generatorArgs = @()
if ($Generator) {
    $generatorArgs = @("-G", $Generator)
}
elseif ($onWindows) {
    # IMPORTANT: prefer Visual Studio generator on Windows so vcpkg x64-windows
    # dependencies match the compiler ABI (MSVC). Falling back to Ninja can pick
    # MinGW g++, which causes link failures with x64-windows packages.
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio/Installer/vswhere.exe"
    $vsInstance = $null
    if (Test-Path $vswhere) {
        $vsInstance = & $vswhere -latest -products * `
            -requires Microsoft.Component.MSBuild `
            -property installationPath
    }

    if ($vsInstance) {
        $generatorArgs = @(
            "-G", "Visual Studio 17 2022",
            "-A", "x64",
            "-DCMAKE_GENERATOR_INSTANCE=$vsInstance"
        )
    }
    else {
        $generatorArgs = @("-G", "Visual Studio 17 2022", "-A", "x64")
    }
}
elseif (Get-Command ninja -ErrorAction SilentlyContinue) {
    $generatorArgs = @("-G", "Ninja")
}

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "[INFO] Cleaning build directory: $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}

$configArgs = @(
    "-S", $repoRoot,
    "-B", $BuildDir,
    "-DCMAKE_BUILD_TYPE=$buildType",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile",
    "-DVCPKG_TARGET_TRIPLET=$triplet",
    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
)
if ($TSan) {
    $configArgs += "-DENABLE_TSAN=ON"
}
if ($cmakeArgs) {
    $configArgs += $cmakeArgs
}

$fluxGraphEnabled = $WithFluxGraph -and -not $WithoutFluxGraph
$configArgs += "-DENABLE_FLUXGRAPH=$(if ($fluxGraphEnabled) { 'ON' } else { 'OFF' })"

if ($fluxGraphEnabled -and $FluxGraphDir) {
    $configArgs += "-DFLUXGRAPH_DIR=$FluxGraphDir"
}
elseif ((-not $fluxGraphEnabled) -and $FluxGraphDir) {
    Write-Host "[WARN] -FluxGraphDir ignored because FluxGraph is disabled." -ForegroundColor Yellow
}

$buildArgs = @("--build", $BuildDir, "--config", $buildType, "--parallel")
if ($Jobs -gt 0) {
    $buildArgs += "$Jobs"
}

Write-Host "[INFO] Repo root: $repoRoot"
Write-Host "[INFO] Build dir: $BuildDir"
Write-Host "[INFO] Build type: $buildType"
Write-Host "[INFO] Triplet: $triplet"
Write-Host "[INFO] TSAN: $TSan"
Write-Host "[INFO] FluxGraph: $(if ($fluxGraphEnabled) { 'ON' } else { 'OFF' })"
if ($fluxGraphEnabled -and $FluxGraphDir) {
    Write-Host "[INFO] FluxGraph dir: $FluxGraphDir"
}

& cmake @generatorArgs @configArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& cmake @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[INFO] Build complete" -ForegroundColor Green
