#pragma once

#include <stddef.h>

namespace tweaks {

// Installs both detours. Returns false and fills reasonOut if either the
// audio-layer dispatcher or the transmission constructor could not be patched.
bool Install(char* reasonOut, size_t reasonSize);

} // namespace tweaks
