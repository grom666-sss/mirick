#include "overlay.hpp"
#include "menu.hpp"
#include <windows.h>
#include <d3d9.h>
#include <MinHook.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx9.h>

namespace {
using PresentFn = HRESULT (WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using ResetFn = HRESULT (WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
PresentFn originalPresent{}; ResetFn originalReset{};
WNDPROC originalWndProc{}; HWND gameWindow{}; bool initialized{};

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYUP && wp == VK_INSERT) { Menu::Toggle(); return 0; }
    if (Menu::IsOpen() && ImGui::GetCurrentContext()) {
        ImGui_ImplWin32_WndProcHandler(hwnd,msg,wp,lp);
        if ((msg>=WM_MOUSEFIRST && msg<=WM_MOUSELAST) || (msg>=WM_KEYFIRST && msg<=WM_KEYLAST)) return 1;
    }
    return CallWindowProc(originalWndProc,hwnd,msg,wp,lp);
}
void Init(IDirect3DDevice9* device) {
    D3DDEVICE_CREATION_PARAMETERS cp{}; device->GetCreationParameters(&cp); gameWindow=cp.hFocusWindow;
    originalWndProc=(WNDPROC)SetWindowLongPtr(gameWindow,GWLP_WNDPROC,(LONG_PTR)WndProc);
    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    auto& io=ImGui::GetIO(); io.IniFilename=nullptr; io.LogFilename=nullptr;
    io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/arial.ttf", 16.0f, nullptr, io.Fonts->GetGlyphRangesCyrillic());
    ImGui::StyleColorsDark(); auto& st=ImGui::GetStyle(); st.FrameRounding=4; st.GrabRounding=4; st.ScrollbarRounding=5;
    ImGui_ImplWin32_Init(gameWindow); ImGui_ImplDX9_Init(device); initialized=true;
}
HRESULT WINAPI Present(IDirect3DDevice9* d,const RECT* a,const RECT* b,HWND c,const RGNDATA* e) {
    if (!initialized) Init(d);
    ImGui_ImplDX9_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
    ImGui::GetIO().MouseDrawCursor=Menu::IsOpen(); Menu::Draw();
    ImGui::EndFrame(); ImGui::Render(); ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    return originalPresent(d,a,b,c,e);
}
HRESULT WINAPI Reset(IDirect3DDevice9* d,D3DPRESENT_PARAMETERS* p) {
    if (initialized) ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr=originalReset(d,p);
    if (initialized && SUCCEEDED(hr)) ImGui_ImplDX9_CreateDeviceObjects(); return hr;
}
bool DeviceMethods(void** outPresent, void** outReset) {
    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(wc); wc.style = CS_CLASSDC; wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(nullptr); wc.lpszClassName = "MirickProbe";
    RegisterClassExA(&wc); HWND w=CreateWindowA(wc.lpszClassName,"",WS_OVERLAPPEDWINDOW,0,0,100,100,nullptr,nullptr,wc.hInstance,nullptr);
    auto d3d=Direct3DCreate9(D3D_SDK_VERSION); if(!d3d){DestroyWindow(w);UnregisterClass(wc.lpszClassName,wc.hInstance);return false;}
    D3DPRESENT_PARAMETERS pp{}; pp.Windowed=TRUE;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=w;
    IDirect3DDevice9* dev{}; HRESULT hr=d3d->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,w,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev);
    if(SUCCEEDED(hr)){void** vt=*reinterpret_cast<void***>(dev);*outReset=vt[16];*outPresent=vt[17];dev->Release();}
    d3d->Release();DestroyWindow(w);UnregisterClass(wc.lpszClassName,wc.hInstance);return SUCCEEDED(hr);
}
}
bool Overlay::Install(){
    void *present{},*reset{}; if(!DeviceMethods(&present,&reset)||MH_Initialize()!=MH_OK)return false;
    if(MH_CreateHook(present,&Present,reinterpret_cast<void**>(&originalPresent))!=MH_OK)return false;
    if(MH_CreateHook(reset,&Reset,reinterpret_cast<void**>(&originalReset))!=MH_OK)return false;
    return MH_EnableHook(MH_ALL_HOOKS)==MH_OK;
}
void Overlay::Remove(){
    MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();
    if(initialized){if(gameWindow&&originalWndProc)SetWindowLongPtr(gameWindow,GWLP_WNDPROC,(LONG_PTR)originalWndProc);ImGui_ImplDX9_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();initialized=false;}
}
