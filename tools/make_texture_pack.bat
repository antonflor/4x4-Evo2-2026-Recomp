@echo off
rem evo2026 - build the full HD texture pack for 4x4 Evo 2 on this PC (NVIDIA GPU recommended).
rem
rem Run this from the evo2026\tools folder inside the game folder (or pass the game folder):
rem     make_texture_pack.bat ["C:\Games\4x4 Evo 2"]
rem
rem Needs Python 3.9+ with:  pip install torch --index-url https://download.pytorch.org/whl/cu128
rem                          pip install pillow numpy
rem
rem 1. exports every texture from the game's .POD archives to <game>\textures_src
rem 2. AI-upscales them 4x (Real-ESRGAN general-x4v3) into <game>\textures
rem Textures that already exist in <game>\textures are skipped, so it is safe to re-run.

setlocal
set "TOOLS=%~dp0"
set "GAME=%~1"
if "%GAME%"=="" set "GAME=%TOOLS%..\.."
if not exist "%GAME%\4x42.exe" (
  echo Can't find 4x42.exe in "%GAME%". Pass the game folder as the first argument.
  exit /b 1
)

python --version >nul 2>&1 || (echo Python 3 is not on PATH. & exit /b 1)

echo [1/3] Exporting textures from the .POD archives...
python "%TOOLS%pod_textures.py" "%GAME%" "%GAME%\textures_src" || exit /b 1

echo [2/3] Upscaling (this is the long part; a recent GeForce does it in minutes)...
python "%TOOLS%upscale_textures.py" "%GAME%\textures_src" "%GAME%\textures" || exit /b 1

echo [3/3] Shrinking: opaque textures to JPEG, transparent ones stay PNG...
python "%TOOLS%optimize_textures.py" "%GAME%\textures" || exit /b 1

echo.
echo Done. Start the game: the new textures load automatically.
echo You can delete "%GAME%\textures_src" afterwards.
endlocal
