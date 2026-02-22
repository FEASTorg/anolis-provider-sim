# Anolis Provider Simulator - Local Run Script (Windows)
#
# Usage:
#   .\scripts\run_local.ps1 [-Preset <name>] [-BuildDir <path>] [-- <args...>]
#
# Examples:
#   .\scripts\run_local.ps1 -- --config config/provider-sim.yaml
#   .\scripts\run_local.ps1 -- --config config/test-physics.yaml --sim-server localhost:50051

param(
    [string]$Preset = "",
    [string]$BuildDir = "",
    [Parameter(ValueFromRemainingArguments)]
    [string[]]$ProviderArgs
)

$ErrorActionPreference = "Stop"

if (-not $Preset) {
    if ($env:OS -eq "Windows_NT") {
        $Preset = "dev-windows-release"
    }
    else {
        $Preset = "dev-release"
    }
}
if (($env:OS -eq "Windows_NT") -and $Preset -in @("dev-release", "dev-debug", "dev-release-fluxgraph")) {
    throw "Preset '$Preset' uses Ninja and may select MinGW on Windows. Use 'dev-windows-release', 'dev-windows-debug', 'dev-windows-release-fluxgraph', or 'ci-windows-release'."
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir

# Auto-detect build directory if not specified
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build\$Preset"
}

if (-not (Test-Path $BuildDir)) {
    Write-Host "[ERROR] Build directory not found: $BuildDir" -ForegroundColor Red
    Write-Host "Run: .\scripts\build.ps1 -Preset $Preset" -ForegroundColor Yellow
    exit 1
}

# Detect provider executable
$ProviderExe = $null
$Candidates = @(
    (Join-Path $BuildDir "anolis-provider-sim.exe"),
    (Join-Path $BuildDir "anolis-provider-sim"),
    (Join-Path $BuildDir "Release\anolis-provider-sim.exe"),
    (Join-Path $BuildDir "Release\anolis-provider-sim"),
    (Join-Path $BuildDir "Debug\anolis-provider-sim.exe"),
    (Join-Path $BuildDir "Debug\anolis-provider-sim")
)

foreach ($candidate in $Candidates) {
    if (Test-Path $candidate) {
        $ProviderExe = $candidate
        break
    }
}

if (-not $ProviderExe) {
    Write-Host "[ERROR] Provider executable not found under: $BuildDir" -ForegroundColor Red
    Write-Host "`nRun: .\scripts\build.ps1 -Preset $Preset" -ForegroundColor Yellow
    exit 1
}

# Launch
Write-Host "[INFO] Starting Anolis Provider Simulator..." -ForegroundColor Cyan
Write-Host "[INFO] Preset:          $Preset" -ForegroundColor Gray
Write-Host "[INFO] Build directory: $BuildDir" -ForegroundColor Gray
Write-Host "[INFO] Executable:      $ProviderExe" -ForegroundColor Gray
Write-Host ""
Write-Host "[INFO] Provider communicates via stdin/stdout using protobuf framing." -ForegroundColor White
Write-Host "[INFO] Press Ctrl+C to stop." -ForegroundColor White
Write-Host ""

& $ProviderExe @ProviderArgs
