#!/bin/bash
# Assemble installer/payload/game from your own 4x4 Evo 2 disc image.
#   tools/make_payload.sh "<disc root folder>" [<HD textures folder>]
# <disc root folder> = the mounted/extracted CD (contains 4x42.exe, data1.hdr, data1.cab).
# Needs: python3, a built build/evo2026.dll (make).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DISC="$1"; TEX="${2:-}"
OUT="$REPO/installer/payload/game"
[ -f "$DISC/4x42.exe" ] && [ -f "$DISC/data1.hdr" ] || { echo "not a 4x4 Evo 2 disc: $DISC"; exit 1; }
[ -f "$REPO/build/evo2026.dll" ] || { echo "build the renderer first (make)"; exit 1; }
rm -rf "$OUT"; mkdir -p "$OUT"
python3 -I "$REPO/tools/is5_extract.py" "$DISC/data1.hdr" "$DISC/data1.cab" "$OUT"
cp "$DISC/4x42.exe" "$OUT/"
rm -rf "$OUT/drivers"                                   # 2001 driver-download pages
mv "$OUT/Tridx8.dll" "$OUT/Tridx8.original.dll"
cp "$REPO/build/evo2026.dll" "$OUT/tridx8.dll"
cp "$REPO/installer/evo2026.ini" "$OUT/system/evo2026.ini"
# METAL.INI: our renderer, 32-bit colour, music from MUSIC.POD (no CD)
sed -i -e 's/^gameBPP=.*/gameBPP=32\r/' -e 's/^rendererDLLPath=.*/rendererDLLPath=tridx8.dll\r/' \
       -e 's/^useRedBookAudio=.*/useRedBookAudio=0\r/' "$OUT/system/METAL.INI"
mkdir -p "$OUT/textures"
# PNG and JPEG replacements; never "notrack" (a canvas the game draws every track preview into)
[ -n "$TEX" ] && find "$TEX" -maxdepth 1 -type f \( -iname '*.png' -o -iname '*.jpg' \) ! -iname 'notrack.*' -exec cp -t "$OUT/textures/" {} +
# evo2026 folder: read me, tools, licence, source
E="$OUT/evo2026"; mkdir -p "$E/tools"
cp "$REPO/installer/README.txt" "$E/README.txt"; sed -i 's/\r$//; s/$/\r/' "$E/README.txt"
cp "$REPO/README.md" "$E/README.md"
cp "$REPO/renderer/LICENSE.4x4e" "$E/LICENSE.4x4e.txt"
cp "$REPO/tools/pod_textures.py" "$REPO/tools/upscale_textures.py" "$REPO/tools/optimize_textures.py" "$REPO/tools/make_texture_pack.bat" "$E/tools/"
git -C "$REPO" archive --format=zip -o "$E/evo2026-source.zip" HEAD
echo "payload ready: $OUT ($(du -sh "$OUT" | cut -f1))"
