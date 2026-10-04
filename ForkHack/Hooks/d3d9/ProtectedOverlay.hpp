#pragma once

#include <d3d9.h>
#include <windows.h>

namespace ProtectedOverlay
{
    // Creates a separate top-level render target. OBS Game Capture only sees the
    // game device, while Windows display/window capture excludes this HWND.
    bool Initialize(HWND gameWindow);
    bool BeginFrame();
    void EndFrame();
    void Shutdown();
    HWND GetWindow();
    IDirect3DDevice9* GetDevice();
    // True while the protected device calls a globally hooked D3D9 method.
    bool IsInternalCall();
}
