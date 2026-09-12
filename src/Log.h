#pragma once

namespace log {
void Open(const char* path, bool enabled);
void Close();

void Write(const char* format, ...);

void Once(const char* format, ...);
}
