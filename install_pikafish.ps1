$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$EngineDir = Join-Path $Root "engines\pikafish"
$Archive = Join-Path $EngineDir "Pikafish.2026-01-02.7z"
$Url = "https://github.com/official-pikafish/Pikafish/releases/download/Pikafish-2026-01-02/Pikafish.2026-01-02.7z"

New-Item -ItemType Directory -Force -Path $EngineDir | Out-Null

Write-Host "Downloading Pikafish..."
Invoke-WebRequest -Uri $Url -OutFile $Archive

$SevenZip = Get-Command 7z -ErrorAction SilentlyContinue
if (-not $SevenZip) {
    $SevenZipPath = "C:\Program Files\7-Zip\7z.exe"
    if (Test-Path $SevenZipPath) {
        $SevenZip = @{ Source = $SevenZipPath }
    }
}

if (-not $SevenZip) {
    Write-Host "Downloaded: $Archive"
    Write-Host "Please install 7-Zip or extract this archive into: $EngineDir"
    exit 0
}

& $SevenZip.Source x $Archive "-o$EngineDir" -y

$Exe = Get-ChildItem -Path $EngineDir -Recurse -Filter "*.exe" |
    Where-Object { $_.Name -match "pikafish" } |
    Select-Object -First 1

if ($Exe) {
    Write-Host "Installed Pikafish: $($Exe.FullName)"
} else {
    Write-Host "Archive extracted, but no Pikafish executable was found under: $EngineDir"
}
