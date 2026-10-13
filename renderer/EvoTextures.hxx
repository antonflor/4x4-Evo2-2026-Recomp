/*
evo2026 — texture dumping and HD texture replacement.

[evo2026] in .\system\evo2026.ini:
    DumpTextures=0      1 = save every texture the game uploads to .\textures_dump\
    ReplaceTextures=1   1 = load replacements from .\textures\

Every texture is identified by the game's own name plus a CRC32 of its pixels
(the same image can appear with different palettes, e.g. truck colours):

    textures_dump\<name>__<crc>.png       what the game uploaded

A replacement is picked up from .\textures\ (any sub-folder) if a PNG named
either "<name>__<crc>.png" (this exact variant) or "<name>.png" (every variant)
exists. Replacements may be any size (power of two recommended, e.g. 4x the
original); mip-maps are generated automatically.
*/
#pragma once

#include "Basic.hxx"
#include "Native.Basic.hxx"

#include <d3d9.h>

namespace Evo::Textures
{
    void Initialize(void);

    // Called after the game's texture has been converted to 32-bit ARGB.
    // Returns a replacement texture (caller takes one reference) or NULL.
    IDirect3DTexture9* Process(IDirect3DDevice9* device, const char* name,
        const u32* argb, const u32 dimensions, const BOOL opaque);

    // Releases every cached replacement (device going away).
    void Release(void);
}
