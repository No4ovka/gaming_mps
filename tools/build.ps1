$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\enter.ps1"
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    idf.py -DIDF_TARGET=esp32 build
    if ($LASTEXITCODE -ne 0) { throw 'ESP32 build failed.' }
} finally { Pop-Location }
