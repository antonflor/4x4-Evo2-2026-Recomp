4x4 EVO 2 - 2026 EDITION (evo2026)
===================================

This is 4x4 Evo 2 (Build 139) with the evo2026 renderer. The game keeps its
own 640x480 layout, and everything is drawn at your monitor's native
resolution (1080p, 1440p, 4K) in a borderless window, with 16x anisotropic
filtering and AI-upscaled HD textures.

On widescreen monitors you get true widescreen: you see more of the scenery
at the sides (nothing is stretched), and the race HUD (lap times, map and
gauges) sits out at the screen edges.

Already set up by the installer:
  - evo2026 installed as tridx8.dll (the game only accepts renderers by this
    file name). The original renderer is kept as Tridx8.original.dll.
  - system\METAL.INI: Direct3D renderer, 32-bit colour, 640x480 layout,
    music from the game files (no CD needed)
  - textures\: HD texture pack
  - 4x42.exe marked DPI-aware for your Windows account


SETTINGS  (Start menu > 4x4 Evo 2 2026 > evo2026 Settings)
--------
The file is system\evo2026.ini, section [evo2026]:

  Width / Height   0         output resolution, 0 = monitor's native
  Window           2         0 exclusive fullscreen, 1 window, 2 borderless
  Widescreen       1         1 true widescreen (wider view), 0 classic 4:3
  HudToEdges       1         1 race HUD at the screen edges, 0 keep it in the
                             middle 4:3 area
  Aspect           1         with Widescreen=0: 1 keep 4:3 with side bars,
                             0 stretch to fill
  VSync            1         sync to the monitor's refresh rate
  Anisotropy       16        texture filtering level (0 = off)
  Filter2D         1         1 smooth 2D scaling, 0 sharp pixels
  ReplaceTextures  1         load HD textures from textures\
  DumpTextures     0         1 = save every texture the game uses to
                             textures_dump\ (for making more HD textures)
  Trace            0         1 = write a renderer log to system\evo2026.log

Leave gamePIXX=640 / gamePIXY=480 in system\METAL.INI. The game's font code
only accepts 4:3 heights and stops with "Invalid screen resolution" at 1080.


THE "4x4 SETUP OPTIONS" SCREEN
------------------------------
The original game shows this screen every time it starts. Just press Enter
("Run 4x4" is already selected). Leave Resolution at 640x480 and Renderer at
Direct3D8: evo2026 draws it at your monitor's resolution anyway.

The intro movie and the legal screen run at 640x480 before the renderer
starts, so they may briefly switch your monitor's mode. That's normal.


MORE HD TEXTURES
----------------
The pack in this version covers the menus and UI, the trucks and drivers,
and the Construction Junction track (1,671 textures). The other tracks use
the original textures, still drawn at full resolution.

To (re)build the full pack on an NVIDIA GPU, run evo2026\tools\make_texture_pack.bat
(needs Python 3 with torch + pillow; the script explains). New PNGs in
textures\ load automatically the next time the game starts.
