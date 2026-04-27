param(
    [string]$RepoRoot    = "C:\Users\Public\Documents\GitHub\LV_Py_101",
    [string]$Compiler    = "wsl x86_64-linux-gnu-gcc",
    [switch]$GitCommit,
    [string]$CommitMessage = "chore: add RT-built .so binaries"
)

$ErrorActionPreference = "Stop"

$SrcDir = Join-Path $RepoRoot "c"
$OutDir = Join-Path $RepoRoot "rt-so"

if (-not (Test-Path $SrcDir)) {
    Write-Error "Source directory not found: $SrcDir"
}

if (-not (Test-Path $OutDir)) {
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
}

function Build-So {
    param(
        [string]$BaseName
    )

    $cFile = Join-Path $SrcDir  ($BaseName + ".c")
    $soFile = Join-Path $OutDir ($BaseName + ".so")

    if (-not (Test-Path $cFile)) {
        Write-Error "C source not found: $cFile"
    }

    Write-Host "Building $soFile from $cFile ..."

    $args = @(
        "-O2", "-shared", "-fPIC", "-fvisibility=hidden",
        "-o", $soFile,
        $cFile,
        "-lm"
    )

    & $Compiler @args
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compiler returned exit code $LASTEXITCODE for $BaseName"
    }

    Write-Host "OK: $soFile"
}

# Build all four shared libraries
Build-So "waveform_gen"
Build-So "piano_synth"
Build-So "drum_synth"
Build-So "melody_learn_v3"

Write-Host "All .so files built in $OutDir"

if ($GitCommit) {
    Push-Location $RepoRoot
    try {
        git add rt-so/*.so
        git commit -m $CommitMessage
        git push origin main
        Write-Host "Pushed .so binaries to origin/main."
    } finally {
        Pop-Location
    }
}

Write-Host "Done."
