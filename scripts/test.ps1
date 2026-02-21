#!/usr/bin/env pwsh
# Test anolis-provider-sim suites against a preset build.
#
# Usage:
#   .\scripts\test.ps1 [options]
#
# Options:
#   -Preset <name>      Build preset to test (default: dev-release on Linux/macOS, dev-windows-release on Windows)
#   -Suite <name>       all|smoke|adpp|multi|fault|fluxgraph (default: all)
#   -Python <command>   Python command (default: python)
#   -NoGenerate         Skip protobuf Python binding generation
#   -Help               Show help

[CmdletBinding(PositionalBinding = $false)]
param(
    [string]$Preset = "",
    [ValidateSet("all", "smoke", "adpp", "multi", "fault", "fluxgraph")]
    [string]$Suite = "all",
    [string]$Python = "python",
    [switch]$NoGenerate,
    [switch]$Help,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraArgs
)

$ErrorActionPreference = "Stop"

function Get-DefaultPreset {
    if ($env:OS -eq "Windows_NT") {
        return "dev-windows-release"
    }
    return "dev-release"
}

function Assert-PresetAllowed {
    param([string]$RequestedPreset)

    if (($env:OS -eq "Windows_NT") -and $RequestedPreset -in @("dev-release", "dev-debug", "dev-release-fluxgraph")) {
        throw "Preset '$RequestedPreset' uses Ninja and may select MinGW on Windows. Use 'dev-windows-release', 'dev-windows-debug', 'dev-windows-release-fluxgraph', or 'ci-windows-release'."
    }
}

for ($i = 0; $i -lt $ExtraArgs.Count; $i++) {
    $arg = $ExtraArgs[$i]
    switch -Regex ($arg) {
        '^--help$' { $Help = $true; continue }
        '^--no-generate$' { $NoGenerate = $true; continue }
        '^--preset$' {
            if ($i + 1 -ge $ExtraArgs.Count) { throw "--preset requires a value" }
            $i++
            $Preset = $ExtraArgs[$i]
            continue
        }
        '^--preset=(.+)$' { $Preset = $Matches[1]; continue }
        '^--suite$|^--test$' {
            if ($i + 1 -ge $ExtraArgs.Count) { throw "$arg requires a value" }
            $i++
            $Suite = $ExtraArgs[$i]
            continue
        }
        '^--suite=(.+)$|^--test=(.+)$' {
            $Suite = if ($Matches[1]) { $Matches[1] } else { $Matches[2] }
            continue
        }
        '^--python$' {
            if ($i + 1 -ge $ExtraArgs.Count) { throw "--python requires a value" }
            $i++
            $Python = $ExtraArgs[$i]
            continue
        }
        '^--python=(.+)$' { $Python = $Matches[1]; continue }
        default { throw "Unknown argument: $arg" }
    }
}

if ($Help) {
    Get-Content $MyInvocation.MyCommand.Path | Select-Object -First 24
    exit 0
}

if ($Suite -notin @("all", "smoke", "adpp", "multi", "fault", "fluxgraph")) {
    throw "Invalid suite '$Suite'. Valid values: all, smoke, adpp, multi, fault, fluxgraph"
}

if (-not $Preset) {
    $Preset = Get-DefaultPreset
}
Assert-PresetAllowed -RequestedPreset $Preset

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$buildDir = Join-Path $repoRoot "build\$Preset"
$env:ANOLIS_PROVIDER_SIM_BUILD_DIR = $buildDir

$pythonCmd = Get-Command $Python -ErrorAction SilentlyContinue
if (-not $pythonCmd) {
    $pythonCmd = Get-Command python3 -ErrorAction SilentlyContinue
    if (-not $pythonCmd) {
        throw "Python not found. Install Python or pass -Python."
    }
}

if (-not $NoGenerate) {
    & (Join-Path $scriptDir "generate_proto_python.ps1") -OutputDir $buildDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

& $pythonCmd.Source -c "import google.protobuf" 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Python package 'protobuf' is missing for: $($pythonCmd.Source)" -ForegroundColor Red
    Write-Host "Install it with: $($pythonCmd.Source) -m pip install protobuf" -ForegroundColor Yellow
    exit 1
}

function Resolve-Executable {
    $candidates = @(
        (Join-Path $buildDir "anolis-provider-sim.exe"),
        (Join-Path $buildDir "anolis-provider-sim"),
        (Join-Path $buildDir "Release\anolis-provider-sim.exe"),
        (Join-Path $buildDir "Release\anolis-provider-sim"),
        (Join-Path $buildDir "Debug\anolis-provider-sim.exe"),
        (Join-Path $buildDir "Debug\anolis-provider-sim")
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }
    throw "Could not find anolis-provider-sim executable in '$buildDir'. Build first with .\scripts\build.ps1 -Preset $Preset"
}

function Assert-FluxGraphEnabledBuild {
    $cachePath = Join-Path $buildDir "CMakeCache.txt"
    if (-not (Test-Path $cachePath)) {
        throw "FluxGraph suite requires configured build at '$buildDir'."
    }
    $cacheRaw = Get-Content $cachePath -Raw
    if ($cacheRaw -notmatch "(?m)^ENABLE_FLUXGRAPH:BOOL=ON$") {
        throw "FluxGraph suite requested but preset '$Preset' has ENABLE_FLUXGRAPH=OFF."
    }
}

function Invoke-TestScript {
    param(
        [string]$Name,
        [string[]]$ScriptArgs
    )
    Write-Host "[INFO] Running $Name..."
    & $pythonCmd.Source @ScriptArgs
    if ($LASTEXITCODE -ne 0) {
        throw "$Name failed with exit code $LASTEXITCODE"
    }
}

$env:ANOLIS_PROVIDER_SIM_EXE = Resolve-Executable
Write-Host "[INFO] Using executable: $env:ANOLIS_PROVIDER_SIM_EXE"
Write-Host "[INFO] Using preset: $Preset"

if ($Suite -eq "fluxgraph") {
    Assert-FluxGraphEnabledBuild
}

Push-Location $repoRoot
try {
    if ($Suite -eq "all" -or $Suite -eq "smoke") {
        Invoke-TestScript -Name "smoke test" -ScriptArgs @("tests/test_hello.py")
    }
    if ($Suite -eq "all" -or $Suite -eq "adpp") {
        Invoke-TestScript -Name "ADPP integration tests" -ScriptArgs @("tests/test_adpp_integration.py", "--test", "all")
    }
    if ($Suite -eq "all" -or $Suite -eq "multi") {
        Invoke-TestScript -Name "multi-instance test" -ScriptArgs @("tests/test_multi_instance.py", "--config", "config/multi-tempctl.yaml")
    }
    if ($Suite -eq "fluxgraph") {
        Invoke-TestScript -Name "FluxGraph integration test" -ScriptArgs @("tests/test_fluxgraph_integration.py")
        Invoke-TestScript -Name "multi-provider scenario" -ScriptArgs @("tests/test_multi_provider_scenario.py")
    }
    if ($Suite -eq "all" -or $Suite -eq "fault") {
        Invoke-TestScript -Name "fault injection tests" -ScriptArgs @("tests/test_fault_injection.py", "--test", "all")
    }
    Write-Host "[INFO] Test suite complete" -ForegroundColor Green
}
finally {
    Pop-Location
}
