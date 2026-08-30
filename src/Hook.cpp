#include "Hook.h"

#include <windows.h>
#include <string.h>

namespace hook {
namespace {

constexpr uint8_t kJmpOpcode = 0xE9;
constexpr size_t  kJmpSize   = 5;
constexpr uint8_t kNopOpcode = 0x90;

void WriteRelativeJump(uint8_t* at, const void* destination) {
    const intptr_t relative = reinterpret_cast<intptr_t>(destination) -
                              reinterpret_cast<intptr_t>(at) - kJmpSize;
    at[0] = kJmpOpcode;
    memcpy(at + 1, &relative, sizeof(int32_t));
}

} // namespace

bool InstallDetour(uintptr_t      target,
                   const void*    detour,
                   const uint8_t* expected,
                   size_t         prologueLength,
                   void**         trampolineOut) {
    if (prologueLength < kJmpSize) return false;

    uint8_t* code = reinterpret_cast<uint8_t*>(target);

    if (memcmp(code, expected, prologueLength) != 0) return false;

    uint8_t* trampoline = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, prologueLength + kJmpSize,
                     MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (trampoline == nullptr) return false;

    memcpy(trampoline, code, prologueLength);
    WriteRelativeJump(trampoline + prologueLength, code + prologueLength);
    FlushInstructionCache(GetCurrentProcess(), trampoline, prologueLength + kJmpSize);

    DWORD previousProtect = 0;
    if (!VirtualProtect(code, prologueLength, PAGE_EXECUTE_READWRITE, &previousProtect)) {
        VirtualFree(trampoline, 0, MEM_RELEASE);
        return false;
    }

    WriteRelativeJump(code, detour);
    memset(code + kJmpSize, kNopOpcode, prologueLength - kJmpSize);

    VirtualProtect(code, prologueLength, previousProtect, &previousProtect);
    FlushInstructionCache(GetCurrentProcess(), code, prologueLength);

    *trampolineOut = trampoline;
    return true;
}

} // namespace hook
