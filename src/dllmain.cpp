#include <windows.h>
#include "overlay.hpp"

static DWORD WINAPI Start(LPVOID) { Overlay::Install(); return 0; }
BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        if (HANDLE thread = CreateThread(nullptr, 0, Start, nullptr, 0, nullptr)) CloseHandle(thread);
    } else if (reason == DLL_PROCESS_DETACH) Overlay::Remove();
    return TRUE;
}
