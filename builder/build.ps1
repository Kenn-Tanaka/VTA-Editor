# ==============================================================================
# Variation Tree Analysis Editor - Universal Multi-Environment Builder (build.ps1)
# ==============================================================================

[CmdletBinding()]
param (
    [switch]$SkipNative,
    [switch]$SkipTests,
    [switch]$ForceInstall,
    [string]$OutputDir = ""
)

$ErrorActionPreference = "Stop"

# Determine paths
$BuilderDir = $PSScriptRoot
$RootDir = Split-Path -Parent $BuilderDir
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $ReleaseDir = Join-Path $RootDir "Release"
} else {
    $ReleaseDir = $OutputDir
}

$PackageDir = Join-Path $RootDir "vta-analysis-package"
$BinDir = Join-Path $BuilderDir "bin"

Write-Host "==============================================================================" -ForegroundColor Cyan
Write-Host "   Variation Tree Analysis Editor - Universal Builder v2.0                   " -ForegroundColor Cyan
Write-Host "==============================================================================" -ForegroundColor Cyan
Write-Host " Root Directory   : $RootDir" -ForegroundColor Gray
Write-Host " Release Output   : $ReleaseDir" -ForegroundColor Gray
Write-Host ""

# [Step 1/6] Preflight Check
Write-Host "[1/6] Checking build prerequisites..." -ForegroundColor Yellow

$nodeInstalled = $false
try {
    $nodeVer = & node -v 2>$null
    if ($nodeVer) {
        Write-Host "  [OK] Node.js detected: $nodeVer" -ForegroundColor Green
        $nodeInstalled = $true
    }
} catch {}

if (-not $nodeInstalled) {
    Write-Host "  [ERROR] Node.js is not installed. Please install Node.js (v18+ LTS) from https://nodejs.org/" -ForegroundColor Red
    exit 1
}

$npmInstalled = $false
try {
    $npmVer = & npm -v 2>$null
    if ($npmVer) {
        Write-Host "  [OK] npm detected: v$npmVer" -ForegroundColor Green
        $npmInstalled = $true
    }
} catch {}

if (-not $npmInstalled) {
    Write-Host "  [ERROR] npm is not detected." -ForegroundColor Red
    exit 1
}

$gppInstalled = $false
if (-not $SkipNative) {
    try {
        $gppVer = & g++ --version 2>$null | Select-Object -First 1
        if ($gppVer) {
            Write-Host "  [OK] C++ Compiler detected: $gppVer" -ForegroundColor Green
            $gppInstalled = $true
        }
    } catch {}
}

if (-not $gppInstalled) {
    Write-Host "  [INFO] C++ Compiler (g++) skipped or not found. Using pre-verified static binaries." -ForegroundColor Cyan
}

# [Step 2/6] Dependency Resolution
Write-Host "`n[2/6] Resolving frontend dependencies..." -ForegroundColor Yellow
$nodeModulesDir = Join-Path $RootDir "node_modules"

Push-Location $RootDir
try {
    if ($ForceInstall -or (-not (Test-Path $nodeModulesDir))) {
        Write-Host "  -> Installing npm packages (npm ci / npm install)..." -ForegroundColor Gray
        if (Test-Path (Join-Path $RootDir "package-lock.json")) {
            & npm ci
        } else {
            & npm install
        }
    } else {
        Write-Host "  [OK] node_modules already exists." -ForegroundColor Green
    }
} finally {
    Pop-Location
}

# [Step 3/6] Automated Unit Tests
if (-not $SkipTests) {
    Write-Host "`n[3/6] Running automated unit tests (npm test)..." -ForegroundColor Yellow
    Push-Location $RootDir
    try {
        & npm test
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  [ERROR] Automated tests failed. Aborting build." -ForegroundColor Red
            exit $LASTEXITCODE
        }
        Write-Host "  [OK] All unit tests passed successfully." -ForegroundColor Green
    } finally {
        Pop-Location
    }
} else {
    Write-Host "`n[3/6] Automated tests skipped (-SkipTests)." -ForegroundColor Gray
}

# [Step 4/6] Frontend Production Build
Write-Host "`n[4/6] Building production frontend bundle (tsc + vite build)..." -ForegroundColor Yellow
Push-Location $RootDir
try {
    & npm run build
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  [ERROR] Frontend build failed." -ForegroundColor Red
        exit $LASTEXITCODE
    }
    Write-Host "  [OK] Production frontend built in dist/" -ForegroundColor Green
} finally {
    Pop-Location
}

# [Step 5/6] Native Binary Verification and Compilation
Write-Host "`n[5/6] Verifying and compiling native binaries..." -ForegroundColor Yellow

if ($gppInstalled -and (-not $SkipNative)) {
    # 1. Compile vta_starter
    $starterSrc = Join-Path $RootDir "tools\vta_starter\main.cpp"
    $starterOut = Join-Path $BinDir "vta-starter.exe"
    if (Test-Path $starterSrc) {
        Write-Host "  -> Compiling vta-starter.exe from source (static linkage)..." -ForegroundColor Gray
        Push-Location (Join-Path $RootDir "tools\vta_starter")
        try {
            & g++ -O2 -std=c++17 main.cpp -static -static-libgcc -static-libstdc++ -lws2_32 -lshlwapi -o $starterOut
            if ($LASTEXITCODE -eq 0) {
                Write-Host "     [OK] vta-starter.exe compiled successfully" -ForegroundColor Green
            } else {
                Write-Host "     [WARN] vta-starter.exe compile failed. Using bundled binary." -ForegroundColor Yellow
            }
        } finally {
            Pop-Location
        }
    }

    # 2. Compile llm_test_cli
    $cliSrc = Join-Path $RootDir "tools\llm_test_cli\main.cpp"
    $cliOut = Join-Path $BinDir "llm_test.exe"
    if (Test-Path $cliSrc) {
        Write-Host "  -> Compiling llm_test.exe from source (static linkage)..." -ForegroundColor Gray
        Push-Location (Join-Path $RootDir "tools\llm_test_cli")
        try {
            & g++ -O2 -std=c++17 main.cpp -static -static-libgcc -static-libstdc++ -lwinhttp -o $cliOut
            if ($LASTEXITCODE -eq 0) {
                Write-Host "     [OK] llm_test.exe compiled successfully" -ForegroundColor Green
            } else {
                Write-Host "     [WARN] llm_test.exe compile failed. Using bundled binary." -ForegroundColor Yellow
            }
        } finally {
            Pop-Location
        }
    }
}

# Verify required binaries and runtime DLLs
$requiredBinaries = @(
    "llm-gateway.exe",
    "llm-gateway-credential-manager.exe",
    "vta-starter.exe",
    "llm_test.exe",
    "libcrypto-3-x64__.dll",
    "libssl-3-x64__.dll",
    "zlib1__.dll"
)

foreach ($bin in $requiredBinaries) {
    $targetPath = Join-Path $BinDir $bin
    if (-not (Test-Path $targetPath)) {
        Write-Host "  [ERROR] Required binary not found: $bin" -ForegroundColor Red
        exit 1
    }
}
Write-Host "  [OK] All native static binaries and OpenSSL runtime DLLs verified." -ForegroundColor Green

# [Step 6/6] Clean Packaging and Release Generation
Write-Host "`n[6/6] Generating distribution packages into Release/..." -ForegroundColor Yellow

if (-not (Test-Path $ReleaseDir)) {
    New-Item -ItemType Directory -Path $ReleaseDir -Force | Out-Null
}

if (-not (Test-Path $PackageDir)) {
    New-Item -ItemType Directory -Path $PackageDir -Force | Out-Null
}

# Cleanly update dist inside package
$packageDist = Join-Path $PackageDir "dist"
if (Test-Path $packageDist) {
    Remove-Item -Recurse -Force $packageDist
}
Copy-Item -Recurse -Force (Join-Path $RootDir "dist") $packageDist

# Copy binaries and DLLs
foreach ($bin in $requiredBinaries) {
    Copy-Item -Force (Join-Path $BinDir $bin) (Join-Path $PackageDir $bin)
}

# Copy scripts and configs
$commonFiles = @(
    "start.bat",
    "credential-manager.bat",
    "test-gateway.bat",
    "LICENSE"
)
foreach ($f in $commonFiles) {
    $src = Join-Path $RootDir $f
    if (Test-Path $src) {
        Copy-Item -Force $src (Join-Path $PackageDir $f)
    }
}

# Manuals live in docs/ but are placed flat in the distribution package
foreach ($f in @("INSTALL_MANUAL.md", "QUICKSTART.md")) {
    $src = Join-Path $RootDir "docs\$f"
    if (Test-Path $src) {
        Copy-Item -Force $src (Join-Path $PackageDir $f)
    }
}

# Distribute the sanitized example config as gateway.json (never the private one)
$exampleCfg = Join-Path $RootDir "gateway.example.json"
if (Test-Path $exampleCfg) {
    Copy-Item -Force $exampleCfg (Join-Path $PackageDir "gateway.json")
}

# 1. Generate App Distribution Zip in Release/
$appZip = Join-Path $ReleaseDir "vta-analysis-package.zip"
if (Test-Path $appZip) {
    Remove-Item -Force $appZip
}
Write-Host "  -> Generating app distribution zip (vta-analysis-package.zip)..." -ForegroundColor Gray
Compress-Archive -Path "$PackageDir\*" -DestinationPath $appZip -CompressionLevel Optimal
$appZipSize = [math]::Round(((Get-Item $appZip).Length / 1MB), 2)
Write-Host "  [OK] Application package generated: $appZipSize MB" -ForegroundColor Green

# 2. Generate Builder Distribution Zip in Release/
$builderZip = Join-Path $ReleaseDir "vta-builder-package.zip"
if (Test-Path $builderZip) {
    Remove-Item -Force $builderZip
}
Write-Host "  -> Generating builder distribution zip (vta-builder-package.zip)..." -ForegroundColor Gray

# Staging area for builder package
$stagingDir = Join-Path $RootDir "scratch\builder-staging"
if (Test-Path $stagingDir) {
    Remove-Item -Recurse -Force $stagingDir
}
New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null

$itemsToInclude = @(
    "src",
    "public",
    "tools",
    "scripts",
    "builder",
    "gateway.example.json",
    "docs",
    "package.json",
    "package-lock.json",
    "tsconfig.json",
    "tsconfig.app.json",
    "tsconfig.node.json",
    "vite.config.ts",
    "vitest.config.ts",
    "index.html",
    "README.md",
    "AGENTS.md",
    "LICENSE",
    "start.bat",
    "build.bat",
    "credential-manager.bat",
    "test-gateway.bat",
    ".gitignore"
)

foreach ($item in $itemsToInclude) {
    $srcItem = Join-Path $RootDir $item
    if (Test-Path $srcItem) {
        Copy-Item -Recurse -Force $srcItem (Join-Path $stagingDir $item)
    }
}

# Exclude heavy temporary build artifacts inside tools to ensure fast and compact packaging
$stagingTools = Join-Path $stagingDir "tools"
if (Test-Path $stagingTools) {
    Get-ChildItem -Path $stagingTools -Recurse -File -Filter "gateway.json" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
    Get-ChildItem -Path $stagingTools -Recurse -Directory -Filter "build" -ErrorAction SilentlyContinue | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    Get-ChildItem -Path $stagingTools -Recurse -File -Filter "*.log" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
}

Compress-Archive -Path "$stagingDir\*" -DestinationPath $builderZip -CompressionLevel Optimal
$builderZipSize = [math]::Round(((Get-Item $builderZip).Length / 1MB), 2)
Write-Host "  [OK] Builder package generated: $builderZipSize MB" -ForegroundColor Green

# Cleanup staging
Remove-Item -Recurse -Force $stagingDir

Write-Host "`n==============================================================================" -ForegroundColor Cyan
Write-Host "   Build and packaging completed successfully!                                " -ForegroundColor Cyan
Write-Host "==============================================================================" -ForegroundColor Cyan
Write-Host "  [Output 1] App Distribution Zip    : $appZip ($appZipSize MB)" -ForegroundColor Green
Write-Host "  [Output 2] Builder Distribution Zip: $builderZip ($builderZipSize MB)" -ForegroundColor Green
Write-Host "==============================================================================" -ForegroundColor Cyan
