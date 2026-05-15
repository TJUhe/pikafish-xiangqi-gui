$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "build-release"
$InstallRoot = Join-Path $Root "dist"
$PackageDir = Join-Path $InstallRoot "pikafish-xiangqi-gui"
$ZipPath = Join-Path $InstallRoot "pikafish-xiangqi-gui-windows-x64.zip"

if (Test-Path $PackageDir) {
    Remove-Item -LiteralPath $PackageDir -Recurse -Force
}

cmake -S $Root -B $BuildDir -DCMAKE_BUILD_TYPE=Release
cmake --build $BuildDir --config Release
cmake --install $BuildDir --config Release --prefix $PackageDir

if (Test-Path $ZipPath) {
    Remove-Item -LiteralPath $ZipPath -Force
}

Compress-Archive -Path (Join-Path $PackageDir "*") -DestinationPath $ZipPath

Write-Host ""
Write-Host "Release folder: $PackageDir"
Write-Host "Release zip:    $ZipPath"
