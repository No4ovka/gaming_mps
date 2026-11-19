param([switch]$NoBuild, [switch]$Debug)
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\enter.ps1"
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    $python = "$env:IDF_PYTHON_ENV_PATH\Scripts\python.exe"
    $arguments = @("$PSScriptRoot\run.py")
    if ($NoBuild) { $arguments += '--no-build' }
    if ($Debug) { $arguments += '--debug' }
    & $python @arguments
    if ($LASTEXITCODE -ne 0) { throw 'QEMU launcher failed.' }
} finally { Pop-Location }
