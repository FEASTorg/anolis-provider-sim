#!/usr/bin/env pwsh
# Generate Python protobuf bindings for test scripts

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$protoFile = Join-Path $repoRoot "external\anolis\spec\device-provider\protocol.proto"
$protoPath = Join-Path $repoRoot "external\anolis\spec\device-provider"
$outputDir = Join-Path $repoRoot "build"

Write-Host "Generating Python protobuf bindings..." -ForegroundColor Cyan

# Find protoc - check PATH first, then vcpkg installation
$protoc = Get-Command protoc -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source
if (-not $protoc) {
    # Check vcpkg installed location
    $vcpkgProtoc = Join-Path $outputDir "vcpkg_installed\x64-windows\tools\protobuf\protoc.exe"
    if (Test-Path $vcpkgProtoc) {
        $protoc = $vcpkgProtoc
        Write-Host "  Using vcpkg protoc: $protoc" -ForegroundColor Gray
    } else {
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
    Write-Host "✓ Generated: $outputFile" -ForegroundColor Green
} else {
    Write-Host "✗ protoc failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit 1
}
