#!/usr/bin/env pwsh
# Test anolis-provider-sim (PowerShell)
#
# Usage:
#   .\scripts\test.ps1 [options]
#
# Options:
#   -Suite <name>          Test suite: all|smoke|adpp|multi|fault (default: all)
#   -BuildDir <path>       Build directory (default: build)
#   -TSan                  Use build-tsan as build directory
#   -Exe <path>            Path to anolis-provider-sim executable
#   -Python <command>      Python command (default: python)
#   -NoGenerate            Skip protobuf Python binding generation
#   -Help                  Show this help

[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet("all", "smoke", "adpp", "multi", "fault")]
    [string]$Suite = "all",
    [string]$BuildDir,
    [switch]$TSan,
    [string]$Exe,
    [string]$Python = "python",
    [switch]$NoGenerate,
    [switch]$Help,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ExtraArgs
)

$ErrorActionPreference = "Stop"

# Support GNU-style long options (e.g. --suite all, --no-generate).
for ($i = 0; $i -lt $ExtraArgs.Count; $i++) {
    $arg = $ExtraArgs[$i]
    switch -Regex ($arg) {
        '^--help$' { $Help = $true; continue }
        '^--tsan$' { $TSan = $true; continue }
        '^--no-generate$' { $NoGenerate = $true; continue }
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
        '^--build-dir$' {
            if ($i + 1 -ge $ExtraArgs.Count) { throw "--build-dir requires a value" }
            $i++
            $BuildDir = $ExtraArgs[$i]
            continue
        }
        '^--build-dir=(.+)$' { $BuildDir = $Matches[1]; continue }
        '^--exe$' {
            if ($i + 1 -ge $ExtraArgs.Count) { throw "--exe requires a value" }
            $i++
            $Exe = $ExtraArgs[$i]
            continue
        }
        '^--exe=(.+)$' { $Exe = $Matches[1]; continue }
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

if ($Suite -notin @("all", "smoke", "adpp", "multi", "fault")) {
    throw "Invalid suite '$Suite'. Valid values: all, smoke, adpp, multi, fault"
}

if ($Help) {
    Get-Content $MyInvocation.MyCommand.Path | Select-Object -First 35
    exit 0
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir

if (-not $BuildDir) {
    $BuildDir = Join-Path $repoRoot ($(if ($TSan) { "build-tsan" } else { "build" }))
}
if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repoRoot $BuildDir
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$env:ANOLIS_PROVIDER_SIM_BUILD_DIR = $BuildDir

$pythonCmd = Get-Command $Python -ErrorAction SilentlyContinue
if (-not $pythonCmd) {
    $pythonCmd = Get-Command python3 -ErrorAction SilentlyContinue
    if (-not $pythonCmd) {
        Write-Host "[ERROR] Python not found. Install Python or pass -Python." -ForegroundColor Red
        exit 1
    }
}

if (-not $NoGenerate) {
    & (Join-Path $scriptDir "generate-proto-python.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

# Validate Python protobuf runtime for the selected interpreter.
& $pythonCmd.Source -c "import google.protobuf" 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Host "[ERROR] Python package 'protobuf' is missing for: $($pythonCmd.Source)" -ForegroundColor Red
    Write-Host "Install it with:" -ForegroundColor Yellow
    Write-Host "  $($pythonCmd.Source) -m pip install protobuf" -ForegroundColor Yellow
    exit 1
}

function Resolve-Executable {
    if ($Exe) {
        $resolved = Resolve-Path -LiteralPath $Exe -ErrorAction SilentlyContinue
        if (-not $resolved) {
            throw "Executable not found: $Exe"
        }
        return $resolved.Path
    }

    $candidates = @(
        (Join-Path $BuildDir "anolis-provider-sim.exe"),
        (Join-Path $BuildDir "Release\anolis-provider-sim.exe"),
        (Join-Path $BuildDir "Debug\anolis-provider-sim.exe"),
        (Join-Path $BuildDir "anolis-provider-sim"),
        (Join-Path $BuildDir "Release\anolis-provider-sim"),
        (Join-Path $BuildDir "Debug\anolis-provider-sim"),
        (Join-Path $repoRoot "build-tsan\anolis-provider-sim")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "Could not find anolis-provider-sim executable. Build first with .\scripts\build.ps1"
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

$exePath = Resolve-Executable
$env:ANOLIS_PROVIDER_SIM_EXE = $exePath
Write-Host "[INFO] Using executable: $env:ANOLIS_PROVIDER_SIM_EXE"

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
    if ($Suite -eq "all" -or $Suite -eq "fault") {
        Invoke-TestScript -Name "fault injection tests" -ScriptArgs @("tests/test_fault_injection.py", "--test", "all")
    }
    Write-Host "[INFO] Test suite complete" -ForegroundColor Green
}
finally {
    Pop-Location
}
