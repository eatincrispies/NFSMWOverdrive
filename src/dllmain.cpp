#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "NFSMW/transmission.hpp"

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        DisableThreadLibraryCalls(module);
        Overdrive::Init(module);
        break;

    case DLL_PROCESS_DETACH:
        if (reserved == nullptr) Overdrive::Restore();
        break;
    }
    return TRUE;
}
