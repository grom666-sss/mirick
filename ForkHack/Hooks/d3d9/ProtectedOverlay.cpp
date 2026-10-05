#include "ProtectedOverlay.hpp"

#include <dwmapi.h>

#include "imgui.h"
#include "imgui_impl_dx9.h"

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

namespace
{
    constexpr wchar_t kOverlayClass[] = L"ForkHackProtectedOverlay";

    HWND gameWindow = nullptr;
    HWND overlayWindow = nullptr;
    IDirect3D9* d3d = nullptr;
    IDirect3DDevice9* device = nullptr;
    D3DPRESENT_PARAMETERS params{};
    int currentWidth = 0;
    int currentHeight = 0;
    bool classRegistered = false;
    thread_local bool internalCall = false;

    LRESULT CALLBACK OverlayWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (message == WM_ERASEBKGND)
        {
            return 1;
        }

        return DefWindowProcW(window, message, wParam, lParam);
    }

    bool GetGameClientBounds(POINT& origin, int& width, int& height)
    {
        RECT client{};
        if (!gameWindow || !IsWindow(gameWindow) || !GetClientRect(gameWindow, &client))
        {
            return false;
        }

        origin = { client.left, client.top };
        if (!ClientToScreen(gameWindow, &origin))
        {
            return false;
        }

        width = client.right - client.left;
        height = client.bottom - client.top;
        return width > 0 && height > 0;
    }

    bool ApplyCaptureProtection(HWND window)
    {
        // Do not silently fall back to WDA_MONITOR: a full-screen transparent
        // overlay can become an opaque rectangle in the recording on old
        // Windows builds. If true exclusion is unavailable, fail closed.
        return SetWindowDisplayAffinity(window, WDA_EXCLUDEFROMCAPTURE) != FALSE;
    }

    bool ResetDevice(int width, int height)
    {
        if (!device)
        {
            return false;
        }

        ImGui_ImplDX9_InvalidateDeviceObjects();
        params.BackBufferWidth = width;
        params.BackBufferHeight = height;
        internalCall = true;
        const HRESULT result = device->Reset(&params);
        internalCall = false;
        if (FAILED(result))
        {
            return false;
        }

        currentWidth = width;
        currentHeight = height;
        ImGui_ImplDX9_CreateDeviceObjects();
        return true;
    }
}

bool ProtectedOverlay::Initialize(HWND targetGameWindow)
{
    gameWindow = targetGameWindow;

    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = OverlayWndProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512)); // IDC_ARROW
    windowClass.lpszClassName = kOverlayClass;

    classRegistered = RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    if (!classRegistered)
    {
        return false;
    }

    POINT origin{};
    int width = 0;
    int height = 0;
    if (!GetGameClientBounds(origin, width, height))
    {
        return false;
    }

    overlayWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kOverlayClass,
        L"ForkHack protected overlay",
        WS_POPUP,
        origin.x,
        origin.y,
        width,
        height,
        nullptr,
        nullptr,
        windowClass.hInstance,
        nullptr);

    if (!overlayWindow)
    {
        return false;
    }

    SetLayeredWindowAttributes(overlayWindow, 0, 255, LWA_ALPHA);

    MARGINS margins{ -1, -1, -1, -1 };
    if (HMODULE dwm = LoadLibraryW(L"dwmapi.dll"))
    {
        using ExtendFrameFn = HRESULT(WINAPI*)(HWND, const MARGINS*);
        if (auto extendFrame = reinterpret_cast<ExtendFrameFn>(GetProcAddress(dwm, "DwmExtendFrameIntoClientArea")))
        {
            extendFrame(overlayWindow, &margins);
        }
        FreeLibrary(dwm);
    }

    if (!ApplyCaptureProtection(overlayWindow))
    {
        Shutdown();
        return false;
    }

    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d)
    {
        Shutdown();
        return false;
    }

    params = {};
    params.Windowed = TRUE;
    params.SwapEffect = D3DSWAPEFFECT_DISCARD;
    params.hDeviceWindow = overlayWindow;
    params.BackBufferFormat = D3DFMT_A8R8G8B8;
    params.BackBufferWidth = width;
    params.BackBufferHeight = height;
    params.EnableAutoDepthStencil = FALSE;
    params.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    HRESULT result = d3d->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        overlayWindow,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
        &params,
        &device);

    if (FAILED(result))
    {
        result = d3d->CreateDevice(
            D3DADAPTER_DEFAULT,
            D3DDEVTYPE_HAL,
            overlayWindow,
            D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
            &params,
            &device);
    }

    if (FAILED(result))
    {
        Shutdown();
        return false;
    }

    currentWidth = width;
    currentHeight = height;
    ShowWindow(overlayWindow, SW_SHOWNOACTIVATE);
    UpdateWindow(overlayWindow);
    return true;
}

bool ProtectedOverlay::BeginFrame()
{
    if (!device || !overlayWindow)
    {
        return false;
    }

    POINT origin{};
    int width = 0;
    int height = 0;
    if (!GetGameClientBounds(origin, width, height))
    {
        ShowWindow(overlayWindow, SW_HIDE);
        return false;
    }

    const bool gameIsForeground = GetForegroundWindow() == gameWindow;
    if (IsIconic(gameWindow) || !IsWindowVisible(gameWindow) || !gameIsForeground)
    {
        ShowWindow(overlayWindow, SW_HIDE);
        return false;
    }

    SetWindowPos(
        overlayWindow,
        HWND_TOPMOST,
        origin.x,
        origin.y,
        width,
        height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW);

    if ((width != currentWidth || height != currentHeight) && !ResetDevice(width, height))
    {
        return false;
    }

    const HRESULT cooperative = device->TestCooperativeLevel();
    if (cooperative == D3DERR_DEVICELOST)
    {
        return false;
    }
    if (cooperative == D3DERR_DEVICENOTRESET && !ResetDevice(width, height))
    {
        return false;
    }

    device->Clear(0, nullptr, D3DCLEAR_TARGET, D3DCOLOR_ARGB(0, 0, 0, 0), 1.0f, 0);
    return SUCCEEDED(device->BeginScene());
}

void ProtectedOverlay::EndFrame()
{
    if (!device)
    {
        return;
    }

    device->EndScene();
    internalCall = true;
    device->Present(nullptr, nullptr, nullptr, nullptr);
    internalCall = false;
}

void ProtectedOverlay::Shutdown()
{
    if (device)
    {
        device->Release();
        device = nullptr;
    }
    if (d3d)
    {
        d3d->Release();
        d3d = nullptr;
    }
    if (overlayWindow)
    {
        DestroyWindow(overlayWindow);
        overlayWindow = nullptr;
    }
    if (classRegistered)
    {
        UnregisterClassW(kOverlayClass, GetModuleHandleW(nullptr));
        classRegistered = false;
    }
}

HWND ProtectedOverlay::GetWindow()
{
    return overlayWindow;
}

IDirect3DDevice9* ProtectedOverlay::GetDevice()
{
    return device;
}

bool ProtectedOverlay::IsInternalCall()
{
    return internalCall;
}
