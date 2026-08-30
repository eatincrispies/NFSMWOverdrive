/*
 *  NFSMW Overdrive
 *
 *  An ASI plugin for Need for Speed: Most Wanted (2005, PC).
 *
 *  Five-speed cars get their 6th gear from the first transmission package
 *  instead of the last.
 *
 *  Reference build: speed.exe, 6,029,312 bytes,
 *  MD5 C0516B485065FABDD69579816B5DF763
 */

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "Config.h"
#include "Game.h"
#include "Log.h"
#include "Tweaks.h"

namespace {

HMODULE g_module  = nullptr;
bool    g_started = false;

// "<the folder this .asi lives in>\<name>". Everything the plugin reads or
// writes sits beside the .asi, so it works wherever the loader puts it.
void ModulePath(char* out, size_t size, const char* name) {
    char folder[MAX_PATH] = {};
    GetModuleFileNameA(g_module, folder, sizeof(folder));

    if (char* lastSlash = strrchr(folder, '\\')) {
        *(lastSlash + 1) = '\0';
    }

    _snprintf_s(out, size, _TRUNCATE, "%s%s", folder, name);
}

void Complain(const char* detail) {
    char message[512];
    _snprintf_s(message, sizeof(message), _TRUNCATE,
                "NFSMW Overdrive could not attach:\n\n%s\n\n"
                "The plugin has disabled itself; the game will run unmodified.",
                detail);
    MessageBoxA(nullptr, message, "NFSMW Overdrive", MB_ICONWARNING | MB_OK);
}

void Start() {
    // Most MW loaders just LoadLibrary the .asi; some also call InitializeASI.
    // Guard so the setup runs exactly once either way.
    if (g_started) return;
    g_started = true;

    char iniPath[MAX_PATH];
    char logPath[MAX_PATH];
    ModulePath(iniPath, sizeof(iniPath), "NFSMWOverdrive.ini");
    ModulePath(logPath, sizeof(logPath), "NFSMWOverdrive.log");

    const config::Settings& settings = config::Load(iniPath);
    log::Open(logPath, settings.log);

    char reason[256] = {};

    // Check the bytes we are about to patch before touching anything, so a
    // different executable fails here instead of crashing later.
    if (!game::VerifyBuild(reason, sizeof(reason))) {
        log::Write("build check failed: %s", reason);
        Complain(reason);
        log::Close();
        return;
    }

    if (!tweaks::Install(reason, sizeof(reason))) {
        log::Write("install failed: %s", reason);
        Complain(reason);
        log::Close();
    }
}

} // namespace

// ASI entry point, called by loaders that look for it.
extern "C" __declspec(dllexport) void InitializeASI() {
    Start();
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            g_module = instance;
            DisableThreadLibraryCalls(instance);
            Start();
            break;

        case DLL_PROCESS_DETACH:
            log::Close();
            break;

        default:
            break;
    }

    return TRUE;
}
