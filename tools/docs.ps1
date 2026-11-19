param([string]$Doxygen)
$ErrorActionPreference = 'Stop'
if (!$Doxygen) {
    $command = Get-Command doxygen.exe -ErrorAction SilentlyContinue
    if ($command) { $Doxygen = $command.Source }
    elseif (Test-Path 'D:\Tools\doxygen\1.18.0\doxygen.exe') {
        $Doxygen = 'D:\Tools\doxygen\1.18.0\doxygen.exe'
    } else { throw 'Install portable Windows Doxygen, or pass -Doxygen <path>.' }
}
$env:PATHEXT = [Environment]::GetEnvironmentVariable('PATHEXT', 'Machine')
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    & $Doxygen Doxyfile
    if ($LASTEXITCODE -ne 0) { throw 'Doxygen reported errors or missing documentation.' }
} finally { Pop-Location }
