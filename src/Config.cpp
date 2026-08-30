#include "Config.h"

#include <windows.h>
#include <string.h>

namespace config {
namespace {

Settings g_settings;

bool ReadBool(const char* section, const char* key, bool fallback,
              const char* iniPath) {
    char value[32] = {};
    if (GetPrivateProfileStringA(section, key, "", value, sizeof(value), iniPath) == 0 ||
        value[0] == '\0') {
        return fallback;
    }
    return _stricmp(value, "1")   == 0 || _stricmp(value, "true") == 0 ||
           _stricmp(value, "yes") == 0 || _stricmp(value, "on")   == 0;
}

} // namespace

const Settings& Load(const char* iniPath) {
    g_settings.sixthGear    = ReadBool("Gears", "Enabled",      true,  iniPath);
    g_settings.includeStock = ReadBool("Gears", "IncludeStock", false, iniPath);
    g_settings.log          = ReadBool("Debug", "Log",          false, iniPath);
    g_settings.verbose      = ReadBool("Debug", "Verbose",      false, iniPath);
    return g_settings;
}

const Settings& Get() { return g_settings; }

} // namespace config
