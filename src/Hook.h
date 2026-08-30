#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hook {

// Installs a 5-byte JMP detour at `target`.
//
// `prologueLength` must be the exact number of bytes of the original prologue
// to relocate (>= 5, and a whole number of instructions). Those bytes are
// copied into a freshly allocated trampoline followed by a JMP back to
// target + prologueLength, and `trampolineOut` receives its address so the
// detour can still invoke the original.
//
// Returns false without touching anything if the bytes at `target` do not
// match `expected` - that keeps a mismatched game build from being patched.
bool InstallDetour(uintptr_t      target,
                   const void*    detour,
                   const uint8_t* expected,
                   size_t         prologueLength,
                   void**         trampolineOut);

} // namespace hook
