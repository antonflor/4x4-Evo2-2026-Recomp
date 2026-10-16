<#
Restores the original 4x4 Evo 2 Direct3D 8 renderer.

    powershell -ExecutionPolicy Bypass -File .\uninstall.ps1 -GamePath "C:\Games\4x4 Evo 2"

Leaves your textures folder and the [evo2026] settings in place (they're ignored by the
original renderer). Delete them by hand if you want a completely clean folder.
#>
param([Parameter(Mandatory = $true)][string]$GamePath)

$ErrorActionPreference = 'Stop'
$GamePath = $GamePath.Trim('"')
$backup = Join-Path $GamePath 'Tridx8.original.dll'
$target = Join-Path $GamePath 'tridx8.dll'

if (-not (Test-Path $backup)) { throw "No Tridx8.original.dll in '$GamePath' - nothing to restore." }

Copy-Item $backup $target -Force
Write-Host 'Original renderer restored (tridx8.dll).'

$bridge = Join-Path $GamePath 'dinput.dll'
if (Test-Path $bridge) {
    Remove-Item $bridge -Force
    Write-Host 'Removed the controller bridge (dinput.dll).'
}

$layers = 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers'
$exe = Join-Path $GamePath '4x42.exe'
if (Get-ItemProperty -Path $layers -Name $exe -ErrorAction SilentlyContinue) {
    Remove-ItemProperty -Path $layers -Name $exe
    Write-Host 'Removed the DPI compatibility flag.'
}
