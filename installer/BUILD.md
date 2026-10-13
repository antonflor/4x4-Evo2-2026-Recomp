# Building the all-in-one installer

`4x4Evo2-2026.iss` is an Inno Setup 6.4+ script. It packs `installer/payload/game/`, which
`tools/make_payload.sh` builds from your own disc:

    make                                              # build/evo2026.dll
    tools/make_payload.sh "<disc folder>" [<HD textures folder>]
    ISCC.exe installer/4x4Evo2-2026.iss               # on Windows, or under Wine

The payload contains the game files from the disc's InstallShield cabinet plus `4x42.exe`
from the disc root, the original renderer kept as `Tridx8.original.dll`, evo2026 as
`tridx8.dll`, a pre-set `system\METAL.INI` (tridx8.dll, 32-bit, music from MUSIC.POD),
`system\evo2026.ini`, the HD `textures\` and an `evo2026\` folder (read me, tools, source).
The 2001 `drivers\` help pages are left out.

Output: `installer/output/4x4Evo2-2026-Setup.exe` + `-N.bin` slices (24 MB each; keep them
together). Game files are never committed to this repo.
