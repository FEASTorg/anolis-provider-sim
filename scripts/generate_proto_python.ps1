#!/usr/bin/env pwsh
# Generate Python protobuf bindings for test scripts

param(
    [string]$OutputDir
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
if (-not $env:ANOLIS_PROTOCOL_PROTO_DIR) {
    Write-Host "ERROR: ANOLIS_PROTOCOL_PROTO_DIR is not set" -ForegroundColor Red
    exit 1
}
$protoRoot = $env:ANOLIS_PROTOCOL_PROTO_DIR
$protoV1Dir = Join-Path $protoRoot "anolis\deviceprovider\v1"
$outputDir = if ($OutputDir) {
    if ([System.IO.Path]::IsPathRooted($OutputDir)) {
        $OutputDir
    } else {
        Join-Path $repoRoot $OutputDir
    }
} elseif ($env:ANOLIS_PROVIDER_SIM_BUILD_DIR) {
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
    # Resolve triplet candidates from CMake cache/env, then probe common fallbacks.
    $triplets = @()
    $cacheCandidates = @(
        (Join-Path $outputDir "CMakeCache.txt"),
        (Join-Path $repoRoot "build\CMakeCache.txt")
    )
    foreach ($cachePath in $cacheCandidates) {
        if (-not (Test-Path $cachePath)) {
            continue
        }
        $tripletLine = Select-String -Path $cachePath -Pattern '^VCPKG_TARGET_TRIPLET:STRING=(.+)$' | Select-Object -First 1
        if ($tripletLine) {
            $cacheTriplet = $tripletLine.Matches[0].Groups[1].Value.Trim()
            if ($cacheTriplet -and ($triplets -notcontains $cacheTriplet)) {
                $triplets += $cacheTriplet
            }
        }
    }

    if ($env:VCPKG_DEFAULT_TRIPLET -and ($triplets -notcontains $env:VCPKG_DEFAULT_TRIPLET)) {
        $triplets += $env:VCPKG_DEFAULT_TRIPLET
    }
    foreach ($defaultTriplet in @("x64-windows-v143", "x64-windows")) {
        if ($triplets -notcontains $defaultTriplet) {
            $triplets += $defaultTriplet
        }
    }

    $vcpkgCandidates = @()
    foreach ($triplet in $triplets) {
        $vcpkgCandidates += (Join-Path $outputDir "vcpkg_installed\$triplet\tools\protobuf\protoc.exe")
        $vcpkgCandidates += (Join-Path $repoRoot "build\vcpkg_installed\$triplet\tools\protobuf\protoc.exe")
    }
    foreach ($vcpkgRoot in @(
            (Join-Path $outputDir "vcpkg_installed"),
            (Join-Path $repoRoot "build\vcpkg_installed")
        )) {
        if (-not (Test-Path $vcpkgRoot)) {
            continue
        }
        foreach ($tripletDir in (Get-ChildItem -Path $vcpkgRoot -Directory -ErrorAction SilentlyContinue)) {
            $candidate = Join-Path $tripletDir.FullName "tools\protobuf\protoc.exe"
            if ($vcpkgCandidates -notcontains $candidate) {
                $vcpkgCandidates += $candidate
            }
        }
    }

    foreach ($candidate in $vcpkgCandidates) {
        if (Test-Path $candidate) {
            $protoc = $candidate
            Write-Host "  Using vcpkg protoc: $protoc" -ForegroundColor Gray
            break
        }
    }

    if (-not $protoc) {
        Write-Host "ERROR: protoc not found in PATH or vcpkg installation" -ForegroundColor Red
        Write-Host "  Checked triplets: $($triplets -join ', ')" -ForegroundColor Yellow
        Write-Host "Install Protocol Buffers compiler from: https://github.com/protocolbuffers/protobuf/releases" -ForegroundColor Yellow
        exit 1
    }
}

# Check if split proto directory exists
if (-not (Test-Path $protoV1Dir)) {
    Write-Host "ERROR: Protocol directory not found: $protoV1Dir" -ForegroundColor Red
    exit 1
}

$protoFiles = Get-ChildItem -Path $protoV1Dir -Filter "*.proto" -File | Sort-Object Name
if (-not $protoFiles) {
    Write-Host "ERROR: No .proto files found in: $protoV1Dir" -ForegroundColor Red
    exit 1
}

$protoArgs = @()
foreach ($proto in $protoFiles) {
    $relative = $proto.FullName.Substring($protoRoot.Length + 1).Replace('\', '/')
    $protoArgs += $relative
}

# Create output directory if needed
if (-not (Test-Path $outputDir)) {
    New-Item -ItemType Directory -Path $outputDir | Out-Null
}

# Generate Python bindings
Write-Host "  Proto dir: $protoV1Dir"
Write-Host "  Output dir: $outputDir"

& $protoc --python_out=$outputDir --proto_path=$protoRoot @protoArgs

if ($LASTEXITCODE -eq 0) {
    $compatFile = Join-Path $outputDir "protocol_pb2.py"
    $compatContent = @'
"""Compatibility shim for legacy protocol_pb2 imports.

Generated from split ADPP v1 protos in anolis/deviceprovider/v1.
"""

from anolis.deviceprovider.v1.call_pb2 import *
from anolis.deviceprovider.v1.envelope_pb2 import *
from anolis.deviceprovider.v1.handshake_pb2 import *
from anolis.deviceprovider.v1.health_pb2 import *
from anolis.deviceprovider.v1.inventory_pb2 import *
from anolis.deviceprovider.v1.readiness_pb2 import *
from anolis.deviceprovider.v1.status_pb2 import *
from anolis.deviceprovider.v1.telemetry_pb2 import *
from anolis.deviceprovider.v1.types_pb2 import *
from anolis.deviceprovider.v1.value_pb2 import *
'@
    Set-Content -Path $compatFile -Value $compatContent -Encoding UTF8

    Write-Host "[OK] Generated Python protos under: $(Join-Path $outputDir 'anolis\deviceprovider\v1')" -ForegroundColor Green
    Write-Host "[OK] Generated compatibility shim: $compatFile" -ForegroundColor Green
} else {
    Write-Host "[FAIL] protoc failed with exit code $LASTEXITCODE" -ForegroundColor Red
    exit 1
}
