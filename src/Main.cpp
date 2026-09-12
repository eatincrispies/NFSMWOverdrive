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
    if (g_started) return;
    g_started = true;

    char iniPath[MAX_PATH];
    char logPath[MAX_PATH];
    ModulePath(iniPath, sizeof(iniPath), "NFSMWOverdrive.ini");
    ModulePath(logPath, sizeof(logPath), "NFSMWOverdrive.log");

    const config::Settings& settings = config::Load(iniPath);
    log::Open(logPath, settings.log);

    char reason[256] = {};

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
}

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
