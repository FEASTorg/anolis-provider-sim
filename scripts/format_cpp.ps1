<#
.SYNOPSIS
    Run clang-format over C++ sources.

.DESCRIPTION
    Formats selected directories relative to repo root.
    Defaults to all if no switches are provided.
#>

param(
    [switch]$Examples,
    [switch]$Src,
    [switch]$Tests
)

$ErrorActionPreference = "Stop"

# Resolve repo root (script located in scripts/)
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

$Dirs = @{
    Examples = Join-Path $RepoRoot "examples"
    Src      = Join-Path $RepoRoot "src"
    Tests    = Join-Path $RepoRoot "tests"
}

# Determine targets (default: all)
$Targets = if (-not ($Examples -or $Src -or $Tests)) {
    $Dirs.Values
} else {
    @(
        if ($Examples) { $Dirs.Examples }
        if ($Src)      { $Dirs.Src }
        if ($Tests)    { $Dirs.Tests }
    )
}

$Total = 0

foreach ($dir in $Targets) {
    if (Test-Path $dir) {
        $Files = Get-ChildItem $dir -Recurse -Include *.cpp,*.hpp -File
        if ($Files) {
            Write-Host "Formatting $($Files.Count) file(s) in $dir"
            $Files | ForEach-Object { clang-format -i $_.FullName }
            $Total += $Files.Count
        }
    }
}

Write-Host ""
Write-Host "Formatted $Total file(s)."
