<#
evo2026 installer for 4x4 Evo 2 (Build 139).

    powershell -ExecutionPolicy Bypass -File .\install.ps1 -GamePath "C:\Games\4x4 Evo 2"

What it does (all reversible with uninstall.ps1):
  * Backs up the game's Tridx8.dll to Tridx8.original.dll (once).
  * Installs evo2026.dll as tridx8.dll, and the XInput controller bridge as dinput.dll. The game only accepts renderers by these exact
    file names, and the Direct3D 8 slot is the one evo2026 implements.
  * Sets system\METAL.INI to that renderer, 32-bit colour, 640x480 logical resolution
    (evo2026 draws that at your monitor's native resolution), and music from the game
    files instead of CD audio.
  * Unpacks any textures-*.zip next to this script into the game's textures folder.
  * Writes default settings to system\evo2026.ini (keeps any values already there).
  * Marks 4x42.exe as DPI-aware for your user account so Windows doesn't blur it.
#>
param(
    [string]$GamePath,
    [switch]$NoCompat
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path

function Find-Game {
    $candidates = @(
        'C:\Games\4x4 Evo 2',
        (Join-Path ${env:ProgramFiles(x86)} 'Terminal Reality\4x4 Evo 2'),
        (Join-Path $env:ProgramFiles 'Terminal Reality\4x4 Evo 2'),
        (Join-Path ${env:ProgramFiles(x86)} 'Gathering\4x4 Evo 2'),
        'C:\Program Files\Terminal Reality\4x4 Evo 2'
    )
    foreach ($c in $candidates) { if ($c -and (Test-Path (Join-Path $c '4x42.exe'))) { return $c } }
    return $null
}

function Set-IniValue([string]$Path, [string]$Section, [string]$Key, [string]$Value) {
    $lines = New-Object System.Collections.Generic.List[string]
    if (Test-Path $Path) { foreach ($l in [System.IO.File]::ReadAllLines($Path)) { $lines.Add($l) } }

    $start = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i].Trim() -ieq "[$Section]") { $start = $i; break }
    }
    if ($start -lt 0) {
        if ($lines.Count -gt 0 -and $lines[$lines.Count - 1].Trim() -ne '') { $lines.Add('') }
        $lines.Add("[$Section]"); $lines.Add("$Key=$Value")
    } else {
        $end = $lines.Count
        for ($i = $start + 1; $i -lt $lines.Count; $i++) { if ($lines[$i].Trim().StartsWith('[')) { $end = $i; break } }
        $found = $false
        for ($i = $start + 1; $i -lt $end; $i++) {
            if ($lines[$i] -match "^\s*$([regex]::Escape($Key))\s*=") { $lines[$i] = "$Key=$Value"; $found = $true; break }
        }
        if (-not $found) {
            $insert = $end
            while ($insert -gt $start + 1 -and $lines[$insert - 1].Trim() -eq '') { $insert-- }
            $lines.Insert($insert, "$Key=$Value")
        }
    }
    [System.IO.File]::WriteAllLines($Path, $lines.ToArray(), [System.Text.Encoding]::ASCII)
}

function Get-IniValue([string]$Path, [string]$Section, [string]$Key) {
    if (-not (Test-Path $Path)) { return $null }
    $inSection = $false
    foreach ($l in [System.IO.File]::ReadAllLines($Path)) {
        $t = $l.Trim()
        if ($t.StartsWith('[')) { $inSection = ($t -ieq "[$Section]"); continue }
        if ($inSection -and $t -match "^$([regex]::Escape($Key))\s*=(.*)$") { return $Matches[1].Trim() }
    }
    return $null
}

function Add-IniDefaults([string]$Path, [string]$Section, [System.Collections.IDictionary]$Values) {
    foreach ($k in $Values.Keys) {
        # only add keys the user hasn't set already
        if ($null -eq (Get-IniValue $Path $Section $k)) { Set-IniValue $Path $Section $k $Values[$k] }
    }
}

if (-not $GamePath) { $GamePath = Find-Game }
if (-not $GamePath) { $GamePath = Read-Host 'Folder that contains 4x42.exe' }
$GamePath = $GamePath.Trim('"')
$exe = Join-Path $GamePath '4x42.exe'
if (-not (Test-Path $exe)) { throw "4x42.exe not found in '$GamePath'." }

Write-Host "4x4 Evo 2: $GamePath"

if ((Get-Item $exe).Length -ne 2776064) {
    Write-Warning 'This 4x42.exe is not the retail Build 139 evo2026 was built for. Native resolution will still work; widescreen will stay off.'
}

$programFolders = @($env:ProgramFiles, ${env:ProgramFiles(x86)}) | Where-Object { $_ }
if ($programFolders | Where-Object { $GamePath -like "$_*" }) {
    Write-Warning 'The game is under Program Files. Windows redirects its saves/settings to a VirtualStore copy; this script updates both, but installing to C:\Games is simpler.'
}

$ourDll = Join-Path $here 'bin\evo2026.dll'
if (-not (Test-Path $ourDll)) { throw "Missing $ourDll" }
$target = Join-Path $GamePath 'tridx8.dll'
$backup = Join-Path $GamePath 'Tridx8.original.dll'

if ((Test-Path $target) -and -not (Test-Path $backup)) {
    if ((Get-FileHash $target).Hash -ne (Get-FileHash $ourDll).Hash) {
        Copy-Item $target $backup
        Write-Host 'Backed up the original renderer to Tridx8.original.dll'
    }
}
Copy-Item $ourDll $target -Force
Write-Host 'Installed evo2026 renderer as tridx8.dll'

# XInput controller bridge (dinput.dll): modern pads with separate gas/brake triggers
$bridge = Join-Path $here 'bin\dinput.dll'
if (Test-Path $bridge) {
    Copy-Item $bridge (Join-Path $GamePath 'dinput.dll') -Force
    Write-Host 'Installed the controller bridge (dinput.dll)'
}

$systemDirs = @(Join-Path $GamePath 'system')
$virtual = $GamePath -replace '^[A-Za-z]:\\', ''
$vs = Join-Path $env:LOCALAPPDATA ("VirtualStore\" + $virtual + "\system")
if (Test-Path $vs) { $systemDirs += $vs }

foreach ($dir in $systemDirs) {
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir | Out-Null }

    $metal = Get-ChildItem $dir -Filter 'metal.ini' -ErrorAction SilentlyContinue | Select-Object -First 1
    $metalPath = if ($metal) { $metal.FullName } else { Join-Path $dir 'METAL.INI' }
    if (Test-Path $metalPath) {
        Set-IniValue $metalPath 'Graphics' 'rendererDLLPath' 'tridx8.dll'
        Set-IniValue $metalPath 'Graphics' 'useDirect3D' '1'
        Set-IniValue $metalPath 'Graphics' 'gameBPP' '32'
        Set-IniValue $metalPath 'Graphics' 'gamePIXX' '640'
        Set-IniValue $metalPath 'Graphics' 'gamePIXY' '480'
        Set-IniValue $metalPath 'Game' 'useRedBookAudio' '0'
        Write-Host "Updated $metalPath"
    }

    # The game rewrites render.ini on exit, so evo2026 keeps its settings in a file of its own.
    $evoPath = Join-Path $dir 'evo2026.ini'
    Add-IniDefaults $evoPath 'evo2026' ([ordered]@{
        Width = '0'; Height = '0'; Window = '2'; Aspect = '1'; VSync = '1'
        Anisotropy = '16'; Filter2D = '1'; Widescreen = '1'; HudToEdges = '1'
        DumpTextures = '0'; ReplaceTextures = '1'; Trace = '0'
    })
    Write-Host "Updated $evoPath"
}

$textures = Join-Path $GamePath 'textures'
if (-not (Test-Path $textures)) { New-Item -ItemType Directory -Path $textures | Out-Null }
$pack = Join-Path $here 'textures'
if (Test-Path $pack) {
    Copy-Item (Join-Path $pack '*') $textures -Recurse -Force
    Write-Host "Copied HD textures to $textures"
}
$zips = @(Get-ChildItem $here -Filter 'textures-*.zip' -ErrorAction SilentlyContinue)
foreach ($z in $zips) {
    Expand-Archive -Path $z.FullName -DestinationPath $textures -Force
    Write-Host "Unpacked $($z.Name) into $textures"
}

if (-not $NoCompat) {
    $layers = 'HKCU:\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers'
    if (-not (Test-Path $layers)) { New-Item -Path $layers -Force | Out-Null }
    New-ItemProperty -Path $layers -Name $exe -Value '~ HIGHDPIAWARE' -PropertyType String -Force | Out-Null
    Write-Host 'Marked 4x42.exe as DPI-aware (Properties > Compatibility shows this).'
}

Write-Host ''
Write-Host 'Done. Launch 4x42.exe. Settings: system\evo2026.ini (see README).'
