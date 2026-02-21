#!/usr/bin/env pwsh
# Generate Python protobuf bindings for test scripts

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$protoFile = Join-Path $repoRoot "external\anolis-protocol\spec\device-provider\protocol.proto"
$protoPath = Join-Path $repoRoot "external\anolis-protocol\spec\device-provider"
$outputDir = if ($env:ANOLIS_PROVIDER_SIM_BUILD_DIR) {
    if ([System.IO.Path]::IsPathRooted($env:ANOLIS_PROVIDER_SIM_BUILD_DIR)) {
        $env:ANOLIS_PROVIDER_SIM_BUILD_DIR
    } else {
        Join-Path $repoRoot $env:ANOLIS_PROVIDER_SIM_BUILD_DIR
    }
} else {
    Join-Path $repoRoot "build"
}
$outputDir = [System.IO.Path]::GetFullPath($outputDir)

Write-Host "Generating Python protobuf bindings..." -ForegroundColor Cyan

# Find protoc - check PATH first, then vcpkg installation
$protoc = Get-Command protoc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
if (-not $protoc) {
    # Check vcpkg installed location under selected output dir, then default build dir.
    $vcpkgCandidates = @(
        (Join-Path $outputDir "vcpkg_installed\x64-windows\tools\protobuf\protoc.exe"),
        (Join-Path $repoRoot "build\vcpkg_installed\x64-windows\tools\protobuf\protoc.exe")
    )
    foreach ($candidate in $vcpkgCandidates) {
        if (Test-Path $candidate) {
            $protoc = $candidate
            Write-Host "  Using vcpkg protoc: $protoc" -ForegroundColor Gray
            break
        }
    }

    if (-not $protoc) {
        Write-Host "ERROR: protoc not found in PATH or vcpkg installation" -ForegroundColor Red
        Write-Host "Install Protocol Buffers compiler from: https://github.com/protocolbuffers/protobuf/releases" -ForegroundColor Yellow
        exit 1
    }
}

# Check if proto file exists
if (-not (Test-Path $protoFile)) {
    Write-Host "ERROR: Protocol file not found: $protoFile" -ForegroundColor Red
    exit 1
}

# Create output directory if needed
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

# Generate Python bindings
Write-Host "  Proto file: $protoFile"
Write-Host "  Output dir: $outputDir"

& $protoc --python_out=$outputDir --proto_path=$protoPath protocol.proto

if ($LASTEXITCODE -eq 0) {
    $outputFile = Join-Path $outputDir "protocol_pb2.py"
    Write-Host "[OK] Generated: $outputFile" -ForegroundColor Green
} else {
    Write-Host "[FAIL] protoc failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit 1
}
