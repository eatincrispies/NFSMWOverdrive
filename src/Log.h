#pragma once

namespace log {

// Opens <module dir>/NFSMWOverdrive.log when [Debug] Log is on. Everything
// below is a cheap no-op otherwise.
void Open(const char* path, bool enabled);
void Close();

void Write(const char* format, ...);

// Writes only the first time a given message is produced. Used inside the
// per-construction hook so the log stays short.
void Once(const char* format, ...);

} // namespace log
