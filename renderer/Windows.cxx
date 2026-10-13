/*
Copyright (c) 2023 Americus Maximus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "Mathematics.Basic.hxx"
#include "Module.hxx"
#include "Trace.hxx"
#include "Evo.hxx"
#include "Quit.hxx"

using namespace Mathematics;
using namespace Renderers::Modules;
using namespace Renderers;
using namespace Settings;

namespace Renderer::Module
{
    extern "C" void AcquireDisplayContext(HDC*) {
        EVO_TRACE("AcquireDisplayContext"); } // NOTE: Not being called by the application.

    extern "C" void ReleaseDisplayContext(void) {
        EVO_TRACE("ReleaseDisplayContext"); } // NOTE: Not being called by the application.

    extern "C" u32 RestoreVideoMode(void)
    {
        EVO_TRACE("RestoreVideoMode");
        DX::Release();

        if (State.DX.DirectX.Device != NULL)
        {
            State.DX.DirectX.Device->Reset(&State.DX.Devices.Current.PresentParams);

            State.DX.DirectX.Device->Release();
            State.DX.DirectX.Device = NULL;
        }

        SHOWCURSOR(TRUE);

        return TRUE;
    }

    extern "C" u32 RestoreVideoModeX(void) {
        EVO_TRACE("RestoreVideoModeX"); return FALSE; } // NOTE: Not being called by the application.

    extern "C" u32 SelectVideoMode(u32 * *pixels)
    {
        EVO_TRACE("SelectVideoMode");
        State.DX.Mode.Mode = RendererMode::None;

        State.DX.Mode.BlendMode = RendererBlendMode::OneMinusSourceAlpha;

        State.DX.Mode.Unk21 = 0; // TODO
        State.DX.Mode.UnknownValue101 = 0.0f; // TODO

        Settings::ReadValue(RENDERER_MODULE_OPTIONS_GRAPHICS_SECTION_NAME,
            RENDERER_MODULE_OPTIONS_GRAPHICS_WINDOW_MODE_NAME, (u32*)&State.Settings.WindowMode);
        Settings::ReadValue(RENDERER_MODULE_OPTIONS_GRAPHICS_SECTION_NAME,
            RENDERER_MODULE_OPTIONS_GRAPHICS_USE_FIXED_FUNCTION_PIPELINE_NAME, (u32*)&State.Settings.FixedPipelineMode);
        Settings::ReadValue(RENDERER_MODULE_OPTIONS_GRAPHICS_SECTION_NAME,
            RENDERER_MODULE_OPTIONS_GRAPHICS_MIP_LEVEL_COUNT_NAME, &State.Settings.MipMapLevelCount);

        Settings::ReadValue(RENDERER_MODULE_OPTIONS_TEXTURES_SECTION_NAME,
            RENDERER_MODULE_OPTIONS_TEXTURES_MIP_MAP_MODE_NAME, &State.Settings.MipMapTextureSize);

        State.Pixels.Pixels = pixels;

        for (u32 x = 0; x < State.Window.Height; x++)
        {
            State.Pixels.Values[x] = pixels[x];
        }

        DX::Release();

        State.DX.DirectX.Instance = Direct3DCreate9(D3D_SDK_VERSION);

        if (State.DX.DirectX.Instance == NULL) { Quit("Unable to initialize DirectX."); }

        State.DX.DirectX.Instance->GetDeviceCaps(State.DX.Devices.Current.Index,
            D3DDEVTYPE::D3DDEVTYPE_HAL, &State.DX.Devices.Current.Capabilities);

        // evo2026: the device always runs at the output resolution (native desktop by default);
        // the game's own resolution becomes a logical coordinate space that Evo:: maps onto it.
        Evo::LoadConfig();

        u32 outWidth = 0, outHeight = 0;
        Evo::PrepareWindow(State.Window.HWND, State.Window.Width, State.Window.Height, &outWidth, &outHeight);
        Evo::ComputeView(State.Window.Width, State.Window.Height, outWidth, outHeight);

        D3DDISPLAYMODE dm = { 0 };
        DX::DXC(State.DX.DirectX.Instance->GetAdapterDisplayMode(State.DX.Devices.Current.Index, &dm),
            "Unable to acquire the desktop display mode.");

        const D3DFORMAT backFormat = D3DFORMAT::D3DFMT_X8R8G8B8;

        auto stencilFormat = D3DFORMAT::D3DFMT_D24S8;
        {
            const D3DFORMAT candidates[] = { D3DFORMAT::D3DFMT_D24S8, D3DFORMAT::D3DFMT_D24X8, D3DFORMAT::D3DFMT_D32, D3DFORMAT::D3DFMT_D16 };
            for (u32 x = 0; x < ARRAYSIZE(candidates); x++)
            {
                if (State.DX.DirectX.Instance->CheckDeviceFormat(State.DX.Devices.Current.Index, D3DDEVTYPE::D3DDEVTYPE_HAL,
                    backFormat, D3DUSAGE_DEPTHSTENCIL, D3DRESOURCETYPE::D3DRTYPE_SURFACE, candidates[x]) == D3D_OK)
                {
                    stencilFormat = candidates[x]; break;
                }
            }
        }

        ZeroMemory(&State.DX.Devices.Current.PresentParams, sizeof(D3DPRESENT_PARAMETERS));

        State.DX.Devices.Current.PresentParams.AutoDepthStencilFormat = stencilFormat;
        State.DX.Devices.Current.PresentParams.BackBufferFormat = backFormat;
        State.DX.Devices.Current.PresentParams.EnableAutoDepthStencil = TRUE;
        State.DX.Devices.Current.PresentParams.hDeviceWindow = State.Window.HWND;
        State.DX.Devices.Current.PresentParams.MultiSampleType = D3DMULTISAMPLE_TYPE::D3DMULTISAMPLE_NONE;
        State.DX.Devices.Current.PresentParams.Windowed = Evo::Config.Window != 0;
        State.DX.Devices.Current.PresentParams.BackBufferCount = 1;
        State.DX.Devices.Current.PresentParams.BackBufferWidth = outWidth;
        State.DX.Devices.Current.PresentParams.BackBufferHeight = outHeight;
        State.DX.Devices.Current.PresentParams.PresentationInterval = Evo::Config.VSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
        State.DX.Devices.Current.PresentParams.FullScreen_RefreshRateInHz = Evo::Config.Window == 0 ? dm.RefreshRate : 0;
        // COPY keeps the side bars and any persistent 2D content intact between frames.
        State.DX.Devices.Current.PresentParams.SwapEffect = D3DSWAPEFFECT::D3DSWAPEFFECT_COPY;

        if (State.DX.DirectX.Instance->CreateDevice(State.DX.Devices.Current.Index, D3DDEVTYPE::D3DDEVTYPE_HAL,
            State.Window.HWND, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
            &State.DX.Devices.Current.PresentParams, &State.DX.DirectX.Device) != D3D_OK
            && State.DX.DirectX.Instance->CreateDevice(State.DX.Devices.Current.Index, D3DDEVTYPE::D3DDEVTYPE_HAL,
            State.Window.HWND, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
            &State.DX.Devices.Current.PresentParams, &State.DX.DirectX.Device) != D3D_OK)
        {
            Quit("Unable to create a Direct3D 9 device at %u x %u.", outWidth, outHeight);
        }

        DX::Initialize();

        SetCursorPos(State.Window.Width - 1, State.Window.Height - 1);

        SHOWCURSOR(FALSE);

        Clear();
        Toggle();

        return TRUE;
    }

    extern "C" u32 SelectVideoMode2(const u32 width, const u32 height, const u32 bpp, u32 * *pixels)
    {
        EVO_TRACE("SelectVideoMode2");
        State.Window.Width = width;
        State.Window.Height = height;
        State.Window.BitsPerPixel = bpp;

        return SelectVideoMode(pixels);
    }

    extern "C" u32 SelectVideoModeX(const u32, const u32, const u32, const u32) {
        EVO_TRACE("SelectVideoModeX"); return FALSE; } // NOTE: Not being called by the application.
}