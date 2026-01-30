# build-installer.ps1
# Build script for VEIL VPN Windows Installer
#
# Usage:
#   .\build-installer.ps1 [-ArtifactsPath <path>] [-Version <version>] [-OutputPath <path>]
#
# Prerequisites:
#   - NSIS 3.x installed (makensis in PATH or at standard location)
#   - Build artifacts available (from CMake build or CI download)
#
# Examples:
#   .\build-installer.ps1                                          # Use defaults
#   .\build-installer.ps1 -ArtifactsPath .\artifacts -Version 1.2.0
#   .\build-installer.ps1 -OutputPath C:\output

[CmdletBinding()]
param(
    [string]$ArtifactsPath = "",
    [string]$Version = "",
    [string]$OutputPath = "",
    [switch]$SkipVersionUpdate
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir

Write-Host "=== VEIL VPN Installer Builder ===" -ForegroundColor Cyan
Write-Host ""

# ---------------------------------------------------------------------------
# 1. Locate NSIS
# ---------------------------------------------------------------------------
function Find-NSIS {
    # Check PATH first
    $makensis = Get-Command makensis -ErrorAction SilentlyContinue
    if ($makensis) {
        return $makensis.Source
    }

    # Check standard installation locations
    $standardPaths = @(
        "C:\Program Files (x86)\NSIS\makensis.exe",
        "C:\Program Files\NSIS\makensis.exe",
        "${env:ProgramFiles(x86)}\NSIS\makensis.exe",
        "${env:ProgramFiles}\NSIS\makensis.exe"
    )

    foreach ($path in $standardPaths) {
        if (Test-Path $path) {
            return $path
        }
    }

    return $null
}

$MakeNSIS = Find-NSIS
if (-not $MakeNSIS) {
    Write-Error "NSIS not found. Please install NSIS 3.x from https://nsis.sourceforge.io/"
    exit 1
}
Write-Host "Found NSIS: $MakeNSIS" -ForegroundColor Green

# ---------------------------------------------------------------------------
# 2. Resolve artifacts path
# ---------------------------------------------------------------------------
if (-not $ArtifactsPath) {
    # Try common locations relative to the installer directory
    $candidates = @(
        (Join-Path $ScriptDir "bin"),
        (Join-Path $RepoRoot "build\src"),
        (Join-Path $RepoRoot "artifacts\bin")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            $ArtifactsPath = Split-Path -Parent $candidate
            # If the candidate is the bin subfolder, use its parent
            if ((Split-Path -Leaf $candidate) -eq "bin") {
                $ArtifactsPath = Split-Path -Parent $candidate
            } else {
                $ArtifactsPath = $candidate
            }
            break
        }
    }

    if (-not $ArtifactsPath) {
        Write-Warning "No artifacts directory found automatically."
        Write-Warning "Please specify -ArtifactsPath pointing to the directory containing bin/ and driver/ subdirectories."
        Write-Warning ""
        Write-Warning "Expected structure:"
        Write-Warning "  <ArtifactsPath>/"
        Write-Warning "    bin/"
        Write-Warning "      veil-client-gui.exe"
        Write-Warning "      veil-vpn.exe"
        Write-Warning "      veil-service.exe"
        Write-Warning "      Qt6Core.dll, ..."
        Write-Warning "    driver/"
        Write-Warning "      wintun.dll"
        exit 1
    }
}

Write-Host "Artifacts path: $ArtifactsPath" -ForegroundColor Green

# ---------------------------------------------------------------------------
# 3. Verify required artifacts exist
# ---------------------------------------------------------------------------
$requiredFiles = @(
    "bin\veil-client-gui.exe"
)
$optionalFiles = @(
    "bin\veil-vpn.exe",
    "bin\veil-service.exe",
    "driver\wintun.dll"
)

$missingRequired = @()
foreach ($file in $requiredFiles) {
    $fullPath = Join-Path $ArtifactsPath $file
    if (-not (Test-Path $fullPath)) {
        $missingRequired += $file
    }
}

if ($missingRequired.Count -gt 0) {
    Write-Error "Missing required artifacts: $($missingRequired -join ', ')"
    exit 1
}

foreach ($file in $optionalFiles) {
    $fullPath = Join-Path $ArtifactsPath $file
    if (-not (Test-Path $fullPath)) {
        Write-Warning "Optional artifact not found: $file"
    }
}

# ---------------------------------------------------------------------------
# 4. Copy artifacts to installer directory layout
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "Copying artifacts to installer directory..." -ForegroundColor Cyan

# Copy bin/ contents
$binSrc = Join-Path $ArtifactsPath "bin"
$binDst = Join-Path $ScriptDir "bin"
if (Test-Path $binSrc) {
    if (Test-Path $binDst) { Remove-Item -Recurse -Force $binDst }
    Copy-Item -Recurse $binSrc $binDst
    Write-Host "  Copied bin/ directory" -ForegroundColor Green
}

# Copy driver/ contents
$driverSrc = Join-Path $ArtifactsPath "driver"
$driverDst = Join-Path $ScriptDir "driver"
if (Test-Path $driverSrc) {
    if (Test-Path $driverDst) { Remove-Item -Recurse -Force $driverDst }
    Copy-Item -Recurse $driverSrc $driverDst
    Write-Host "  Copied driver/ directory" -ForegroundColor Green
}

# Copy docs
$docsDst = Join-Path $ScriptDir "docs"
if (-not (Test-Path $docsDst)) {
    New-Item -ItemType Directory -Force -Path $docsDst | Out-Null
}
$readmeSrc = Join-Path $RepoRoot "README.md"
if (Test-Path $readmeSrc) {
    Copy-Item $readmeSrc $docsDst
    Write-Host "  Copied README.md to docs/" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# 5. Update version in NSIS script (optional)
# ---------------------------------------------------------------------------
$nsiPath = Join-Path $ScriptDir "veil-vpn.nsi"
if (-not (Test-Path $nsiPath)) {
    Write-Error "NSIS script not found at: $nsiPath"
    exit 1
}

if ($Version -and -not $SkipVersionUpdate) {
    Write-Host ""
    Write-Host "Updating version to $Version..." -ForegroundColor Cyan

    $versionParts = $Version.Split('.')
    $major = if ($versionParts.Length -gt 0) { $versionParts[0] } else { "1" }
    $minor = if ($versionParts.Length -gt 1) { $versionParts[1] } else { "0" }
    $patch = if ($versionParts.Length -gt 2) { $versionParts[2].Split('-')[0] } else { "0" }

    $nsiContent = Get-Content $nsiPath -Raw
    $nsiContent = $nsiContent -replace '!define PRODUCT_VERSION ".*"', "!define PRODUCT_VERSION `"$Version`""
    $nsiContent = $nsiContent -replace '!define PRODUCT_VERSION_MAJOR \d+', "!define PRODUCT_VERSION_MAJOR $major"
    $nsiContent = $nsiContent -replace '!define PRODUCT_VERSION_MINOR \d+', "!define PRODUCT_VERSION_MINOR $minor"
    $nsiContent = $nsiContent -replace '!define PRODUCT_VERSION_PATCH \d+', "!define PRODUCT_VERSION_PATCH $patch"
    Set-Content $nsiPath $nsiContent -NoNewline
    Write-Host "  Version updated to $Version ($major.$minor.$patch)" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# 6. Build the installer
# ---------------------------------------------------------------------------
Write-Host ""
Write-Host "Building installer..." -ForegroundColor Cyan

Push-Location $ScriptDir
try {
    & $MakeNSIS veil-vpn.nsi
    if ($LASTEXITCODE -ne 0) {
        Write-Error "NSIS compilation failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }
} finally {
    Pop-Location
}

# ---------------------------------------------------------------------------
# 7. Move output to requested location
# ---------------------------------------------------------------------------
$installerFile = Get-ChildItem -Path $ScriptDir -Filter "veil-vpn-*-setup.exe" | Sort-Object LastWriteTime -Descending | Select-Object -First 1

if (-not $installerFile) {
    Write-Error "Installer executable not found after build."
    exit 1
}

if ($OutputPath) {
    if (-not (Test-Path $OutputPath)) {
        New-Item -ItemType Directory -Force -Path $OutputPath | Out-Null
    }
    $destination = Join-Path $OutputPath $installerFile.Name
    Move-Item -Force $installerFile.FullName $destination
    Write-Host ""
    Write-Host "Installer created: $destination" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "Installer created: $($installerFile.FullName)" -ForegroundColor Green
}

Write-Host ""
Write-Host "=== Build complete ===" -ForegroundColor Cyan
