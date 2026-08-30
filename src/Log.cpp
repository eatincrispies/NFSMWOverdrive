#include "Log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace log {
namespace {

FILE* g_file    = nullptr;
bool  g_enabled = false;

// Small fixed ring of messages already emitted, so Once() needs no allocator
// inside a hot hook.
constexpr int kSlots  = 32;
constexpr int kLength = 128;

char g_seen[kSlots][kLength];
int  g_seenCount = 0;

void Emit(const char* text) {
    if (g_file == nullptr) return;
    fputs(text, g_file);
    fputc('\n', g_file);
    fflush(g_file);
}

bool Format(char* buffer, size_t size, const char* format, va_list args) {
    return _vsnprintf_s(buffer, size, _TRUNCATE, format, args) >= 0;
}

} // namespace

void Open(const char* path, bool enabled) {
    g_enabled = enabled;
    if (!enabled) return;
    if (fopen_s(&g_file, path, "w") != 0) g_file = nullptr;
    Emit("NFSMW Overdrive");
}

void Close() {
    if (g_file != nullptr) {
        fclose(g_file);
        g_file = nullptr;
    }
    g_enabled = false;
}

void Write(const char* format, ...) {
    if (!g_enabled) return;

    char buffer[512];
    va_list args;
    va_start(args, format);
    const bool ok = Format(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (ok) Emit(buffer);
}

void Once(const char* format, ...) {
    if (!g_enabled) return;

    char buffer[512];
    va_list args;
    va_start(args, format);
    const bool ok = Format(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (!ok) return;

    for (int i = 0; i < g_seenCount; ++i) {
        if (strncmp(g_seen[i], buffer, kLength - 1) == 0) return;
    }
    if (g_seenCount < kSlots) {
        strncpy_s(g_seen[g_seenCount], kLength, buffer, _TRUNCATE);
        ++g_seenCount;
    }

    Emit(buffer);
}

} // namespace log
