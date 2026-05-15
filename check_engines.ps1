$ErrorActionPreference = "Continue"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Engines = @(
    @{ Name = "Built-in"; Dir = ""; Pattern = ""; Protocol = "builtin"; Extra = @() },
    @{ Name = "Pikafish"; Dir = "pikafish"; Pattern = "*pikafish*.exe"; Extra = @("setoption name EvalFile value pikafish.nnue") },
    @{ Name = "Fairy-Stockfish"; Dir = "fairy-stockfish"; Pattern = "*.exe"; Extra = @("setoption name UCI_Variant value xiangqi") },
    @{ Name = "ElephantEye"; Dir = "eleeye"; Pattern = "*.exe"; Protocol = "ucci"; Extra = @() },
    @{ Name = "ElephantArt"; Dir = "elephantart"; Pattern = "*.exe"; Protocol = "ucci"; Extra = @() },
    @{ Name = "PX0"; Dir = "px0"; Pattern = "*.exe"; Protocol = "uci"; Extra = @() }
)

foreach ($engine in $Engines) {
    $engineRoot = Join-Path (Join-Path $Root "engines") $engine.Dir
    Write-Host ""
    Write-Host "== $($engine.Name) =="
    if ($engine.Protocol -eq "builtin") {
        Write-Host "Built into xiangqi.exe"
        continue
    }
    if (-not (Test-Path $engineRoot)) {
        Write-Host "Missing directory: $engineRoot"
        continue
    }

    $exe = Get-ChildItem -Path $engineRoot -Recurse -Filter "*.exe" |
        Where-Object { $_.Name -like $engine.Pattern -or $_.Name -match "stockfish|fairy" } |
        Select-Object -First 1

    if (-not $exe) {
        Write-Host "No executable found under: $engineRoot"
        continue
    }

    Write-Host "Executable: $($exe.FullName)"

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $exe.FullName
    $psi.WorkingDirectory = $engineRoot
    $psi.RedirectStandardInput = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow = $true

    $process = [System.Diagnostics.Process]::Start($psi)
    if ($engine.Protocol -eq "ucci") {
        $process.StandardInput.WriteLine("ucci")
        $process.StandardInput.WriteLine("isready")
        $process.StandardInput.WriteLine("position startpos")
        $process.StandardInput.WriteLine("go time 300")
    } else {
        $process.StandardInput.WriteLine("uci")
        foreach ($line in $engine.Extra) {
            $process.StandardInput.WriteLine($line)
        }
        $process.StandardInput.WriteLine("isready")
        $process.StandardInput.WriteLine("position startpos")
        $process.StandardInput.WriteLine("go depth 1")
    }
    $process.StandardInput.WriteLine("quit")
    $process.StandardInput.Close()

    $output = $process.StandardOutput.ReadToEnd()
    $process.WaitForExit(8000) | Out-Null

    $output -split "`r?`n" |
        Where-Object { $_ -match "^(id name|uciok|readyok|bestmove|info string ERROR)" } |
        ForEach-Object { Write-Host $_ }
}
