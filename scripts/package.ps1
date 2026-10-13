# scripts/package.ps1 - Variation Tree Analysis Editor Clean Packaging Script
$ErrorActionPreference = 'Stop'
$rootDir = Split-Path -Parent $PSScriptRoot
$pkgDir = Join-Path $rootDir 'vta-analysis-package'
$distDir = Join-Path $rootDir 'dist'
$pkgDistDir = Join-Path $pkgDir 'dist'
$zipPath = Join-Path $rootDir 'vta-analysis-package.zip'

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Variation Tree Analysis Editor Packaging Script " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# 1. Sync dist cleanly (remove all old build artifacts - M-01)
Write-Host "[1/3] Cleanly copying dist to package directory..." -ForegroundColor Yellow
if (Test-Path $pkgDistDir) {
    Remove-Item -Recurse -Force $pkgDistDir
}
Copy-Item -Recurse -Force $distDir $pkgDistDir

# 2. Verify clean assets
$jsFiles = Get-ChildItem -Path (Join-Path $pkgDistDir 'assets') -Filter "*.js"
$cssFiles = Get-ChildItem -Path (Join-Path $pkgDistDir 'assets') -Filter "*.css"
Write-Host "  -> Copied JS bundles: $($jsFiles.Count)" -ForegroundColor Green
Write-Host "  -> Copied CSS bundles: $($cssFiles.Count)" -ForegroundColor Green

# 3. Create zip archive
Write-Host "[2/3] Creating vta-analysis-package.zip..." -ForegroundColor Yellow
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
Compress-Archive -Path "$pkgDir\*" -DestinationPath $zipPath -Force

# 4. Result display
$zipItem = Get-Item $zipPath
$zipSizeMb = [math]::Round($zipItem.Length / 1MB, 2)
Write-Host "[3/3] Package generated successfully!" -ForegroundColor Green
Write-Host "  -> Archive: $($zipItem.Name)"
Write-Host "  -> Total Size: $zipSizeMb MB"
Write-Host "==========================================" -ForegroundColor Cyan
