# Anolis Provider Simulator - Local Run Script (Windows)
#
# Usage:
#   .\scripts\run_local.ps1 [-BuildDir <path>] [-Config <Release|Debug>] [-- <args...>]
#
# Examples:
#   .\scripts\run_local.ps1 -- --config config/provider-sim.yaml
#   .\scripts\run_local.ps1 -- --config config/test-physics.yaml --flux-server localhost:50051

param(
    [string]$BuildDir = "",
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Config = "Release",
    [Parameter(ValueFromRemainingArguments)]
    [string[]]$ProviderArgs
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir

# Auto-detect build directory if not specified
if (-not $BuildDir) {
    $BuildDir = Join-Path $RepoRoot "build"
}

if (-not (Test-Path $BuildDir)) {
    Write-Host "[ERROR] Build directory not found: $BuildDir" -ForegroundColor Red
    Write-Host "Run: .\scripts\build.ps1" -ForegroundColor Yellow
    exit 1
}

# Detect provider executable
$ProviderExe = Join-Path $BuildDir "$Config\anolis-provider-sim.exe"

if (-not (Test-Path $ProviderExe)) {
    Write-Host "[ERROR] Provider executable not found at:" -ForegroundColor Red
    Write-Host "  $ProviderExe" -ForegroundColor Gray
    Write-Host "`nRun: .\scripts\build.ps1 -Config $Config" -ForegroundColor Yellow
    exit 1
}

# Launch
Write-Host "[INFO] Starting Anolis Provider Simulator..." -ForegroundColor Cyan
Write-Host "[INFO] Build directory: $BuildDir" -ForegroundColor Gray
Write-Host "[INFO] Executable:      $ProviderExe" -ForegroundColor Gray
Write-Host ""
Write-Host "[INFO] Provider communicates via stdin/stdout using protobuf framing." -ForegroundColor White
Write-Host "[INFO] Press Ctrl+C to stop." -ForegroundColor White
Write-Host ""

& $ProviderExe @ProviderArgs
