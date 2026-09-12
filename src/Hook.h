#pragma once

#include <stddef.h>
#include <stdint.h>

namespace hook {
bool InstallDetour(uintptr_t      target,
                   const void*    detour,
                   const uint8_t* expected,
                   size_t         prologueLength,
                   void**         trampolineOut);
}
