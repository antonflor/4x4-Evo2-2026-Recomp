/*
evo2026 — texture dumping and HD texture replacement. See EvoTextures.hxx.
*/
#include "EvoTextures.hxx"
#include "Evo.hxx"
#include "Trace.hxx"
#include "Mathematics.Basic.hxx"
using namespace Mathematics;

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_DEFAULTS
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Evo::Textures
{
    static struct
    {
        BOOL Initialized = FALSE;
        u32 Dump = 0;
        u32 Replace = 1;

        // lower-case file stem -> full path, for every PNG in the textures folder
        std::unordered_map<std::string, std::string> Files;

        // key -> loaded replacement (holds one reference)
        std::unordered_map<std::string, IDirect3DTexture9*> Cache;

        // keys that have no replacement, so we don't keep probing
        std::unordered_set<std::string> Missing;

        std::unordered_set<std::string> Dumped;

        u32 Loaded = 0;
    } Store;

    static u32 Crc32(const u8* data, const size_t length)
    {
        static u32 table[256];
        static BOOL ready = FALSE;

        if (!ready)
        {
            for (u32 i = 0; i < 256; i++)
            {
                u32 c = i;
                for (u32 k = 0; k < 8; k++) { c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); }
                table[i] = c;
            }
            ready = TRUE;
        }

        u32 crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < length; i++) { crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8); }
        return crc ^ 0xFFFFFFFFu;
    }

    static std::string Sanitize(const char* name)
    {
        std::string out;
        for (const char* p = name; *p != 0; p++)
        {
            const char c = *p;
            if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') { out += (char)tolower((unsigned char)c); }
            else { out += '_'; }
        }
        // drop a trailing image extension the game may include (e.g. "foo.raw")
        const auto dot = out.rfind('.');
        if (dot != std::string::npos && out.size() - dot <= 4) { out.erase(dot); }
        if (out.empty()) { out = "unnamed"; }
        return out;
    }

    static void Scan(const std::string& folder)
    {
        WIN32_FIND_DATAA fd;
        const HANDLE h = FindFirstFileA((folder + "\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) { return; }

        do
        {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) { continue; }

            const std::string path = folder + "\\" + fd.cFileName;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { Scan(path); continue; }

            std::string stem = fd.cFileName;
            const auto dot = stem.rfind('.');
            if (dot == std::string::npos) { continue; }

            std::string ext = stem.substr(dot + 1);
            for (auto& c : ext) { c = (char)tolower((unsigned char)c); }
            if (ext != "png") { continue; }

            stem.erase(dot);
            for (auto& c : stem) { c = (char)tolower((unsigned char)c); }

            Store.Files[stem] = path;
        } while (FindNextFileA(h, &fd));

        FindClose(h);
    }

    void Initialize(void)
    {
        if (Store.Initialized) { return; }
        Store.Initialized = TRUE;

        Store.Dump = ReadSetting("DumpTextures", 0);
        Store.Replace = ReadSetting("ReplaceTextures", 1);

        if (Store.Replace) { Scan(".\\textures"); }
        if (Store.Dump) { CreateDirectoryA(".\\textures_dump", NULL); }

        EVO_TRACE("evo2026 textures: dump=%u replace=%u, %u replacement file(s) found",
            Store.Dump, Store.Replace, (u32)Store.Files.size());
    }

    void Release(void)
    {
        for (auto& it : Store.Cache) { if (it.second != NULL) { it.second->Release(); } }
        Store.Cache.clear();
        Store.Missing.clear();
    }

    static void Dump(const std::string& key, const u32* argb, const u32 dims, const BOOL opaque)
    {
        if (Store.Dumped.count(key) != 0) { return; }
        Store.Dumped.insert(key);

        u8* rgba = (u8*)malloc((size_t)dims * dims * 4);
        if (rgba == NULL) { return; }

        for (u32 i = 0; i < dims * dims; i++)
        {
            const u32 c = argb[i];
            rgba[i * 4 + 0] = (u8)(c >> 16);
            rgba[i * 4 + 1] = (u8)(c >> 8);
            rgba[i * 4 + 2] = (u8)(c);
            rgba[i * 4 + 3] = opaque ? 0xff : (u8)(c >> 24);
        }

        const std::string path = ".\\textures_dump\\" + key + ".png";
        stbi_write_png(path.c_str(), (int)dims, (int)dims, 4, rgba, (int)dims * 4);
        free(rgba);
    }

    static IDirect3DTexture9* Load(IDirect3DDevice9* device, const std::string& path, const BOOL opaque)
    {
        FILE* f = fopen(path.c_str(), "rb");
        if (f == NULL) { return NULL; }

        int w = 0, h = 0, n = 0;
        u8* rgba = stbi_load_from_file(f, &w, &h, &n, 4);
        fclose(f);

        if (rgba == NULL || w <= 0 || h <= 0)
        {
            EVO_TRACE("evo2026 textures: could not read %s", path.c_str());
            if (rgba != NULL) { stbi_image_free(rgba); }
            return NULL;
        }

        IDirect3DTexture9* texture = NULL;
        if (device->CreateTexture((UINT)w, (UINT)h, 0, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, NULL) != D3D_OK)
        {
            stbi_image_free(rgba);
            return NULL;
        }

        // Level 0 from the PNG, then box-filtered mips.
        u32 lw = (u32)w, lh = (u32)h;
        u32* level = (u32*)malloc((size_t)lw * lh * 4);

        for (u32 i = 0; i < lw * lh; i++)
        {
            const u8* p = &rgba[i * 4];
            const u32 a = opaque ? 0xff : p[3];
            level[i] = (a << 24) | ((u32)p[0] << 16) | ((u32)p[1] << 8) | (u32)p[2];
        }
        stbi_image_free(rgba);

        const u32 levels = texture->GetLevelCount();

        for (u32 l = 0; l < levels; l++)
        {
            D3DLOCKED_RECT rect;
            if (texture->LockRect(l, &rect, NULL, 0) == D3D_OK)
            {
                for (u32 y = 0; y < lh; y++) { memcpy((u8*)rect.pBits + (size_t)y * rect.Pitch, &level[(size_t)y * lw], (size_t)lw * 4); }
                texture->UnlockRect(l);
            }

            if (l + 1 >= levels) { break; }

            const u32 nw = lw > 1 ? lw / 2 : 1, nh = lh > 1 ? lh / 2 : 1;
            u32* next = (u32*)malloc((size_t)nw * nh * 4);

            for (u32 y = 0; y < nh; y++)
            {
                for (u32 x = 0; x < nw; x++)
                {
                    const u32 x0 = Min<u32>(x * 2, lw - 1), x1 = Min<u32>(x * 2 + 1, lw - 1);
                    const u32 y0 = Min<u32>(y * 2, lh - 1), y1 = Min<u32>(y * 2 + 1, lh - 1);
                    const u32 c[4] = { level[y0 * lw + x0], level[y0 * lw + x1], level[y1 * lw + x0], level[y1 * lw + x1] };
                    u32 out = 0;
                    for (u32 s = 0; s < 32; s += 8)
                    {
                        const u32 v = (((c[0] >> s) & 0xff) + ((c[1] >> s) & 0xff) + ((c[2] >> s) & 0xff) + ((c[3] >> s) & 0xff) + 2) / 4;
                        out |= v << s;
                    }
                    next[y * nw + x] = out;
                }
            }

            free(level);
            level = next; lw = nw; lh = nh;
        }

        free(level);

        Store.Loaded++;
        EVO_TRACE("evo2026 textures: loaded %s (%dx%d)", path.c_str(), w, h);

        return texture;
    }

    IDirect3DTexture9* Process(IDirect3DDevice9* device, const char* name, const u32* argb, const u32 dimensions, const BOOL opaque)
    {
        Initialize();

        if (!Store.Dump && (!Store.Replace || Store.Files.empty())) { return NULL; }

        const u32 crc = Crc32((const u8*)argb, (size_t)dimensions * dimensions * 4);
        const std::string base = Sanitize(name);

        char suffix[16];
        snprintf(suffix, sizeof(suffix), "__%08x", crc);
        const std::string key = base + suffix;

        if (Store.Dump) { Dump(key, argb, dimensions, opaque); }

        if (!Store.Replace || Store.Files.empty()) { return NULL; }

        const auto cached = Store.Cache.find(key);
        if (cached != Store.Cache.end()) { cached->second->AddRef(); return cached->second; }
        if (Store.Missing.count(key) != 0) { return NULL; }

        auto file = Store.Files.find(key);
        if (file == Store.Files.end()) { file = Store.Files.find(base); }
        if (file == Store.Files.end()) { Store.Missing.insert(key); return NULL; }

        IDirect3DTexture9* texture = Load(device, file->second, opaque);
        if (texture == NULL) { Store.Missing.insert(key); return NULL; }

        Store.Cache[key] = texture;
        texture->AddRef();
        return texture;
    }
}
