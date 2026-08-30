#include "Game.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

namespace game {
namespace {

using BStringHashFn = uint32_t(__cdecl*)(const char*);

// First bytes of bStringHash at 0x005CC240.
//   8B 44 24 04   mov  eax, [esp+4]
//   85 C0         test eax, eax
//   74 23         je   .empty
const uint8_t kBStringHashBytes[] = { 0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0, 0x74, 0x23 };

bool RegionHasFlags(const void* address, size_t size, DWORD flags) {
    const uint8_t* cursor = static_cast<const uint8_t*>(address);
    const uint8_t* end    = cursor + size;
    if (cursor == nullptr || end < cursor) return false;

    while (cursor < end) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(cursor, &info, sizeof(info)) != sizeof(info)) return false;
        if (info.State != MEM_COMMIT) return false;
        if (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
        if ((info.Protect & flags) == 0) return false;
        cursor = static_cast<const uint8_t*>(info.BaseAddress) + info.RegionSize;
    }
    return true;
}

} // namespace

bool IsReadable(const void* address, size_t size) {
    return RegionHasFlags(address, size,
                          PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                          PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                          PAGE_EXECUTE_WRITECOPY);
}

bool IsWritable(const void* address, size_t size) {
    return RegionHasFlags(address, size,
                          PAGE_READWRITE | PAGE_WRITECOPY |
                          PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
}

bool SafeRead(const void* address, void* out, size_t size) {
    if (!IsReadable(address, size)) return false;
    memcpy(out, address, size);
    return true;
}

uint32_t BStringHash(const char* text) {
    if (text == nullptr || *text == '\0') return 0;
    return reinterpret_cast<BStringHashFn>(kBStringHash)(text);
}

bool VerifyBuild(char* reasonOut, size_t reasonSize) {
    struct Check {
        const char*    name;
        uintptr_t      address;
        const uint8_t* expected;
        size_t         length;
    };

    const Check checks[] = {
        { "bStringHash",       kBStringHash,      kBStringHashBytes,
          sizeof(kBStringHashBytes) },
        { "transmission ctor", kTransmissionCtor, kTransmissionCtorBytes,
          sizeof(kTransmissionCtorBytes) },
    };

    for (const Check& check : checks) {
        const void* code = reinterpret_cast<const void*>(check.address);
        if (!IsReadable(code, check.length)) {
            _snprintf_s(reasonOut, reasonSize, _TRUNCATE,
                        "%s at 0x%08X is not readable",
                        check.name, static_cast<unsigned>(check.address));
            return false;
        }
        if (memcmp(code, check.expected, check.length) != 0) {
            _snprintf_s(reasonOut, reasonSize, _TRUNCATE,
                        "%s at 0x%08X does not match the reference build",
                        check.name, static_cast<unsigned>(check.address));
            return false;
        }
    }

    // If the game's own hasher does not reproduce a constant lifted straight
    // out of the binary, nothing downstream is trustworthy.
    if (BStringHash("transmission") != kKeyTransmission ||
        BStringHash("default")      != kKeyDefault) {
        _snprintf_s(reasonOut, reasonSize, _TRUNCATE,
                    "bStringHash produced unexpected values");
        return false;
    }

    return true;
}

} // namespace game
