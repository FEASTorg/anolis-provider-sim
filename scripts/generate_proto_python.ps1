#!/usr/bin/env pwsh
# Generate Python protobuf bindings for test scripts

param(
    [string]$OutputDir
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$protoFile = Join-Path $repoRoot "external\anolis-protocol\spec\device-provider\protocol.proto"
$protoPath = Join-Path $repoRoot "external\anolis-protocol\spec\device-provider"
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
